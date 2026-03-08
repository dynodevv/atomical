/*
 * Atomical OS - Kernel Self-Test Framework
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _KERNEL_KTEST_H
#define _KERNEL_KTEST_H

#include <kernel/types.h>

/* Test result type */
typedef struct {
    int total;
    int passed;
    int failed;
} ktest_results_t;

/* Run all kernel self-tests and return results */
ktest_results_t ktest_run_all(void);

/* Run individual test suites */
ktest_results_t ktest_run_strings(void);
ktest_results_t ktest_run_heap(void);
ktest_results_t ktest_run_pmm(void);
ktest_results_t ktest_run_timer(void);
ktest_results_t ktest_run_sched(void);
ktest_results_t ktest_run_vfs(void);

#endif /* _KERNEL_KTEST_H */
