/*
 * Atomical OS - Main Kernel Header
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _KERNEL_KERNEL_H
#define _KERNEL_KERNEL_H

#include <kernel/types.h>

/* Kernel version */
#define ATOMICAL_VERSION_MAJOR  0
#define ATOMICAL_VERSION_MINOR  1
#define ATOMICAL_VERSION_PATCH  0
#define ATOMICAL_VERSION_STRING "0.1.0"
#define ATOMICAL_NAME           "Atomical"

/* Kernel log levels */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4,
} log_level_t;

/* Core kernel functions */
void kprintf(const char *fmt, ...);
void klog(log_level_t level, const char *subsystem, const char *fmt, ...);
NORETURN void panic(const char *fmt, ...);
NORETURN void halt(void);

/* String utilities */
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

/* Architecture-specific initialization */
void arch_init(void);

/* Subsystem initialization */
void mm_init(void);
void sched_init(void);
void vfs_init(void);
void drivers_init(void);

#endif /* _KERNEL_KERNEL_H */
