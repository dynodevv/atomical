/*
 * Atomical OS - Round-Robin Scheduler
 * Timer-driven preemptive multitasking for kernel tasks.
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <kernel/sched.h>
#include <kernel/mm.h>
#include <hal/hal.h>

/* Kernel stack size: 16 KB per task */
#define KERNEL_STACK_SIZE (4 * PAGE_SIZE)

/* Maximum number of tasks */
#define MAX_TASKS 64

/* Time slice in timer ticks (10 ms at 1000 Hz) */
#define DEFAULT_TIME_SLICE 10

/* Task table */
static task_t *task_table[MAX_TASKS];
static task_t *current_task;
static task_t *idle_task;
static pid_t   next_pid = 0;
static int     sched_enabled = 0;

/* External trampoline for new tasks (defined in arch entry.S) */
extern void task_entry_trampoline(void);

/* --- PID Management --- */

pid_t alloc_pid(void)
{
    for (pid_t p = next_pid; p < MAX_TASKS; p++) {
        if (!task_table[p]) {
            next_pid = p + 1;
            return p;
        }
    }
    /* Wrap around */
    for (pid_t p = 1; p < next_pid && p < MAX_TASKS; p++) {
        if (!task_table[p]) {
            next_pid = p + 1;
            return p;
        }
    }
    return -1;
}

task_t *find_task_by_pid(pid_t pid)
{
    if (pid < 0 || pid >= MAX_TASKS)
        return NULL;
    return task_table[pid];
}

/* --- Scheduler Core --- */

/* Idle task function: runs when no other task is ready */
static void idle_entry(void *arg)
{
    (void)arg;
    for (;;) {
        hal_cpu_idle();
    }
}

void sched_init(void)
{
    memset(task_table, 0, sizeof(task_table));

    /* Create idle task (PID 0) */
    idle_task = sched_create_task("idle", idle_entry, NULL, 0);
    if (!idle_task) {
        panic("Failed to create idle task");
    }

    /* The idle task is our first current task */
    current_task = idle_task;
    current_task->state = TASK_RUNNING;

    /* Register timer callback for preemption (IRQ 0 is already registered,
     * so we use a different mechanism: hook into the existing timer) */
    sched_enabled = 1;

    kprintf("[sched] Scheduler initialized (round-robin, %d ms slice)\n",
            DEFAULT_TIME_SLICE);
}

void schedule(void)
{
    if (!sched_enabled)
        return;

    hal_interrupts_disable();

    task_t *prev = current_task;

    /* Find next READY task (round-robin) */
    task_t *next = NULL;
    pid_t start = (prev->pid + 1) % MAX_TASKS;
    for (pid_t i = 0; i < MAX_TASKS; i++) {
        pid_t idx = (start + i) % MAX_TASKS;
        task_t *t = task_table[idx];
        if (t && t->state == TASK_READY) {
            next = t;
            break;
        }
    }

    /* If no ready task found, run idle task */
    if (!next)
        next = idle_task;

    if (next == prev) {
        hal_interrupts_enable();
        return;
    }

    /* Update task states */
    if (prev->state == TASK_RUNNING)
        prev->state = TASK_READY;
    next->state = TASK_RUNNING;
    next->time_slice = DEFAULT_TIME_SLICE;

    current_task = next;

    hal_interrupts_enable();

    /* Perform the context switch */
    hal_context_switch(&prev->context, &next->context);
}

task_t *sched_create_task(const char *name, void (*entry)(void *), void *arg, int priority)
{
    pid_t pid = alloc_pid();
    if (pid < 0) {
        kprintf("[sched] ERROR: No free PID slots\n");
        return NULL;
    }

    task_t *task = kzalloc(sizeof(task_t));
    if (!task) return NULL;

    /* Allocate kernel stack */
    uintptr_t stack_phys = pmm_alloc_frames(KERNEL_STACK_SIZE / PAGE_SIZE);
    if (!stack_phys) {
        kfree(task);
        return NULL;
    }

    task->pid             = pid;
    task->ppid            = current_task ? current_task->pid : 0;
    task->state           = TASK_READY;
    task->priority        = priority;
    task->time_slice      = DEFAULT_TIME_SLICE;
    task->kernel_stack    = PHYS_TO_VIRT(stack_phys);
    task->kernel_stack_size = KERNEL_STACK_SIZE;
    task->vruntime        = 0;
    task->exit_code       = 0;

    strncpy(task->name, name, TASK_NAME_MAX - 1);
    task->name[TASK_NAME_MAX - 1] = '\0';

    /* Set up initial CPU context so context switch starts executing entry(arg) */
    void *stack_top = (void *)((uintptr_t)task->kernel_stack + KERNEL_STACK_SIZE);
    hal_context_init(&task->context, stack_top, entry, arg);

    task_table[pid] = task;

    kprintf("[sched] Created task '%s' (PID %d)\n", name, pid);
    return task;
}

void sched_destroy_task(task_t *task)
{
    if (!task)
        return;

    hal_interrupts_disable();

    task_table[task->pid] = NULL;
    task->state = TASK_DEAD;

    if (task->kernel_stack) {
        pmm_free_frames(VIRT_TO_PHYS(task->kernel_stack),
                        task->kernel_stack_size / PAGE_SIZE);
    }

    kfree(task);
    hal_interrupts_enable();
}

task_t *sched_get_current(void)
{
    return current_task;
}

void sched_yield(void)
{
    if (current_task)
        current_task->time_slice = 0;
    schedule();
}

void sched_sleep(uint64_t milliseconds)
{
    if (!current_task)
        return;

    uint64_t wake_tick = hal_timer_get_ticks() + milliseconds;
    current_task->state = TASK_SLEEPING;

    while (hal_timer_get_ticks() < wake_tick) {
        schedule();
    }

    current_task->state = TASK_RUNNING;
}

void sched_block(task_t *task)
{
    if (!task)
        return;
    task->state = TASK_BLOCKED;
    if (task == current_task)
        schedule();
}

void sched_unblock(task_t *task)
{
    if (!task)
        return;
    task->state = TASK_READY;
}

void sched_exit(int code)
{
    if (!current_task || current_task == idle_task)
        return;

    current_task->exit_code = code;
    current_task->state = TASK_ZOMBIE;

    kprintf("[sched] Task '%s' (PID %d) exited with code %d\n",
            current_task->name, current_task->pid, code);

    schedule();

    /* Should not reach here */
    for (;;)
        hal_cpu_idle();
}

/* System call stub */
int sys_execve(const char *path, char *const argv[], char *const envp[])
{
    (void)path;
    (void)argv;
    (void)envp;
    /* TODO: ELF loader and process execution */
    return -1;
}
