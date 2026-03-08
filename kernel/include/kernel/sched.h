/*
 * Atomical OS - Process / Scheduler Header
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _KERNEL_SCHED_H
#define _KERNEL_SCHED_H

#include <kernel/types.h>
#include <kernel/mm.h>
#include <arch.h>
#include <hal/hal.h>

/* Process states */
typedef enum {
    TASK_RUNNING   = 0,
    TASK_READY     = 1,
    TASK_BLOCKED   = 2,
    TASK_SLEEPING  = 3,
    TASK_ZOMBIE    = 4,
    TASK_DEAD      = 5,
} task_state_t;

/* Maximum process name length */
#define TASK_NAME_MAX 64

/* Task / Process Control Block (PCB) */
typedef struct task {
    pid_t          pid;
    pid_t          ppid;
    char           name[TASK_NAME_MAX];
    task_state_t   state;
    int            exit_code;

    /* Scheduling */
    int            priority;
    uint64_t       vruntime;         /* CFS virtual runtime */
    uint64_t       time_slice;

    /* Memory */
    mm_struct_t   *mm;
    void          *kernel_stack;
    size_t         kernel_stack_size;

    /* CPU context (arch-specific saved registers) */
    cpu_context_t  context;

    /* Credentials */
    uid_t          uid;
    gid_t          gid;

    /* Linked list for scheduler run queue */
    struct task   *next;
    struct task   *prev;
} task_t;

/* Scheduler interface */
void    sched_init(void);
void    schedule(void);
task_t *sched_create_task(const char *name, void (*entry)(void *), void *arg, int priority);
void    sched_destroy_task(task_t *task);
task_t *sched_get_current(void);
void    sched_yield(void);
void    sched_sleep(uint64_t milliseconds);
void    sched_block(task_t *task);
void    sched_unblock(task_t *task);
void    sched_exit(int code);

/* PID management */
pid_t   alloc_pid(void);
task_t *find_task_by_pid(pid_t pid);

/* System call: execve */
int sys_execve(const char *path, char *const argv[], char *const envp[]);

#endif /* _KERNEL_SCHED_H */
