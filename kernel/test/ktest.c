/*
 * Atomical OS - Kernel Self-Test Framework
 * Validates kernel subsystems by running built-in tests at boot.
 * Results are printed to serial and framebuffer console.
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <kernel/ktest.h>
#include <kernel/mm.h>
#include <kernel/vfs.h>
#include <kernel/sched.h>
#include <hal/hal.h>

/* Test assertion helper */
static int test_pass_count;
static int test_fail_count;
static int test_total_count;

static void test_assert(bool condition, const char *test_name, const char *detail)
{
    test_total_count++;
    if (condition) {
        test_pass_count++;
        kprintf("  [PASS] %s: %s\n", test_name, detail);
    } else {
        test_fail_count++;
        kprintf("  [FAIL] %s: %s\n", test_name, detail);
    }
}

/* --- String function tests --- */

static void test_strings(void)
{
    kprintf("\n--- String Functions ---\n");

    /* strlen */
    test_assert(strlen("hello") == 5, "strlen", "basic string length");
    test_assert(strlen("") == 0, "strlen", "empty string");
    test_assert(strlen("a") == 1, "strlen", "single char");

    /* strcmp */
    test_assert(strcmp("abc", "abc") == 0, "strcmp", "equal strings");
    test_assert(strcmp("abc", "abd") < 0, "strcmp", "less than");
    test_assert(strcmp("abd", "abc") > 0, "strcmp", "greater than");
    test_assert(strcmp("", "") == 0, "strcmp", "empty strings");

    /* strncmp */
    test_assert(strncmp("abcdef", "abcxyz", 3) == 0, "strncmp", "partial match");
    test_assert(strncmp("abc", "abd", 2) == 0, "strncmp", "n < diff position");

    /* strcpy */
    char buf[32];
    strcpy(buf, "test");
    test_assert(strcmp(buf, "test") == 0, "strcpy", "basic copy");

    /* strncpy */
    memset(buf, 'X', sizeof(buf));
    strncpy(buf, "hi", 5);
    test_assert(strcmp(buf, "hi") == 0, "strncpy", "copy with padding");
    test_assert(buf[3] == '\0', "strncpy", "null padding");

    /* memcpy */
    char src[] = "hello world";
    char dst[32];
    memcpy(dst, src, 12);
    test_assert(strcmp(dst, "hello world") == 0, "memcpy", "basic copy");

    /* memset */
    memset(buf, 'A', 5);
    buf[5] = '\0';
    test_assert(strcmp(buf, "AAAAA") == 0, "memset", "fill pattern");

    /* memcmp */
    test_assert(memcmp("abc", "abc", 3) == 0, "memcmp", "equal buffers");
    test_assert(memcmp("abc", "abd", 3) < 0, "memcmp", "less than");
}

/* --- Heap allocator tests --- */

static void test_heap(void)
{
    kprintf("\n--- Heap Allocator ---\n");

    /* Basic allocation */
    void *p1 = kmalloc(64);
    test_assert(p1 != NULL, "kmalloc", "64-byte allocation");

    /* Zero allocation */
    void *p2 = kzalloc(128);
    test_assert(p2 != NULL, "kzalloc", "128-byte allocation");

    /* Verify zeroing */
    uint8_t *bytes = (uint8_t *)p2;
    bool all_zero = true;
    for (int i = 0; i < 128; i++) {
        if (bytes[i] != 0) {
            all_zero = false;
            break;
        }
    }
    test_assert(all_zero, "kzalloc", "memory zeroed");

    /* Write and read back */
    memset(p1, 0xAB, 64);
    test_assert(((uint8_t *)p1)[0] == 0xAB, "kmalloc", "write/read 0xAB");
    test_assert(((uint8_t *)p1)[63] == 0xAB, "kmalloc", "write/read at end");

    /* Free and realloc */
    kfree(p1);
    void *p3 = kmalloc(64);
    test_assert(p3 != NULL, "kfree+kmalloc", "reuse after free");

    /* krealloc */
    void *p4 = kmalloc(32);
    test_assert(p4 != NULL, "krealloc", "initial alloc");
    memset(p4, 0xCD, 32);
    void *p5 = krealloc(p4, 128);
    test_assert(p5 != NULL, "krealloc", "grow allocation");
    test_assert(((uint8_t *)p5)[0] == 0xCD, "krealloc", "data preserved");
    test_assert(((uint8_t *)p5)[31] == 0xCD, "krealloc", "data preserved at end");

    /* Cleanup */
    kfree(p2);
    kfree(p3);
    kfree(p5);

    /* Multiple allocations */
    void *ptrs[16];
    bool alloc_ok = true;
    for (int i = 0; i < 16; i++) {
        ptrs[i] = kmalloc(256);
        if (!ptrs[i]) {
            alloc_ok = false;
            break;
        }
    }
    test_assert(alloc_ok, "kmalloc", "16 x 256-byte allocations");

    for (int i = 0; i < 16; i++) {
        if (ptrs[i]) kfree(ptrs[i]);
    }
}

/* --- PMM tests --- */

static void test_pmm(void)
{
    kprintf("\n--- Physical Memory Manager ---\n");

    uint64_t free_before = pmm_get_free_memory();
    test_assert(free_before > 0, "pmm", "free memory > 0");

    uint64_t total = pmm_get_total_memory();
    test_assert(total > 0, "pmm", "total memory > 0");
    test_assert(free_before <= total, "pmm", "free <= total");

    /* Allocate a frame */
    uintptr_t frame = pmm_alloc_frame();
    test_assert(frame != 0, "pmm_alloc_frame", "single frame allocation");

    uint64_t free_after_alloc = pmm_get_free_memory();
    test_assert(free_after_alloc == free_before - PAGE_SIZE,
                "pmm_alloc_frame", "free memory decreased by PAGE_SIZE");

    /* Free the frame */
    pmm_free_frame(frame);
    uint64_t free_after_free = pmm_get_free_memory();
    test_assert(free_after_free == free_before,
                "pmm_free_frame", "free memory restored");

    /* Contiguous allocation */
    uintptr_t frames = pmm_alloc_frames(4);
    test_assert(frames != 0, "pmm_alloc_frames", "4 contiguous frames");
    test_assert(IS_ALIGNED(frames, PAGE_SIZE), "pmm_alloc_frames", "page-aligned");

    pmm_free_frames(frames, 4);
    test_assert(pmm_get_free_memory() == free_before,
                "pmm_free_frames", "freed 4 frames");
}

/* --- Timer tests --- */

static void test_timer(void)
{
    kprintf("\n--- Timer ---\n");

    uint64_t t1 = hal_timer_get_ticks();
    test_assert(true, "timer", "get_ticks callable");

    uint64_t ns = hal_timer_get_ns();
    (void)t1;
    (void)ns;
    test_assert(true, "timer", "get_ns callable");
}

/* --- Scheduler tests --- */

static volatile int task_a_ran = 0;
static volatile int task_b_ran = 0;

static void test_task_a(void *arg)
{
    (void)arg;
    task_a_ran = 1;
    kprintf("  [info] Task A running (PID %d)\n", sched_get_current()->pid);
    sched_exit(0);
}

static void test_task_b(void *arg)
{
    (void)arg;
    task_b_ran = 1;
    kprintf("  [info] Task B running (PID %d)\n", sched_get_current()->pid);
    sched_exit(0);
}

static void test_scheduler(void)
{
    kprintf("\n--- Scheduler ---\n");

    task_t *current = sched_get_current();
    test_assert(current != NULL, "sched", "current task exists");
    test_assert(current->pid == 0, "sched", "idle task is PID 0");

    /* Create test tasks */
    task_t *ta = sched_create_task("test_a", test_task_a, NULL, 1);
    test_assert(ta != NULL, "sched_create_task", "created task A");
    test_assert(ta->pid > 0, "sched_create_task", "task A has valid PID");
    test_assert(ta->state == TASK_READY, "sched_create_task", "task A is READY");

    task_t *tb = sched_create_task("test_b", test_task_b, NULL, 1);
    test_assert(tb != NULL, "sched_create_task", "created task B");

    /* Yield to let test tasks run */
    sched_yield();
    sched_yield();
    sched_yield();

    /* Check if tasks ran */
    test_assert(task_a_ran == 1, "schedule", "task A executed");
    test_assert(task_b_ran == 1, "schedule", "task B executed");
}

/* --- Run all tests --- */

ktest_results_t ktest_run_all(void)
{
    test_pass_count = 0;
    test_fail_count = 0;
    test_total_count = 0;

    kprintf("\n");
    kprintf("============================================\n");
    kprintf("  Atomical OS - Kernel Self-Tests\n");
    kprintf("============================================\n");

    test_strings();
    test_heap();
    test_pmm();
    test_timer();
    test_scheduler();

    kprintf("\n============================================\n");
    kprintf("  Results: %d/%d passed, %d failed\n",
            test_pass_count, test_total_count, test_fail_count);
    kprintf("============================================\n\n");

    ktest_results_t results = {
        .total  = test_total_count,
        .passed = test_pass_count,
        .failed = test_fail_count,
    };

    return results;
}
