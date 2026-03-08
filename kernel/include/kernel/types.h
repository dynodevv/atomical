/*
 * Atomical OS - Core Type Definitions
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _KERNEL_TYPES_H
#define _KERNEL_TYPES_H

/* Standard integer types */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

/* Size and address types */
typedef uint64_t           size_t;
typedef int64_t            ssize_t;
typedef uint64_t           uintptr_t;
typedef int64_t            intptr_t;
typedef int64_t            ptrdiff_t;
typedef int64_t            off_t;
typedef int32_t            pid_t;
typedef uint32_t           uid_t;
typedef uint32_t           gid_t;
typedef uint32_t           mode_t;
typedef uint64_t           ino_t;
typedef int64_t            blkcnt_t;
typedef int64_t            blksize_t;
typedef uint32_t           dev_t;
typedef uint32_t           nlink_t;
typedef int64_t            time_t;

/* Boolean type */
typedef _Bool              bool;
#define true               1
#define false              0

/* Null pointer */
#define NULL               ((void *)0)

/* Compiler attributes */
#define PACKED             __attribute__((packed))
#define ALIGNED(n)         __attribute__((aligned(n)))
#define NORETURN           __attribute__((noreturn))
#define UNUSED             __attribute__((unused))
#define SECTION(s)         __attribute__((section(s)))
#define WEAK               __attribute__((weak))
#define USED               __attribute__((used))

/* Limits */
#define UINT8_MAX          0xFF
#define UINT16_MAX         0xFFFF
#define UINT32_MAX         0xFFFFFFFF
#define UINT64_MAX         0xFFFFFFFFFFFFFFFF
#define INT32_MAX          0x7FFFFFFF
#define INT64_MAX          0x7FFFFFFFFFFFFFFF
#define SIZE_MAX           UINT64_MAX

/* Page size constants */
#define PAGE_SIZE          4096
#define PAGE_SHIFT         12
#define PAGE_MASK          (~(PAGE_SIZE - 1))

/* Alignment macros */
#define ALIGN_UP(x, a)     (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a)   ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a)   (((x) & ((a) - 1)) == 0)

/* Min/Max macros */
#define MIN(a, b)          ((a) < (b) ? (a) : (b))
#define MAX(a, b)          ((a) > (b) ? (a) : (b))

/* Bit manipulation */
#define BIT(n)             (1ULL << (n))
#define BITS_PER_LONG      64

/* Container of macro */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - __builtin_offsetof(type, member)))

/* Array size */
#define ARRAY_SIZE(arr)    (sizeof(arr) / sizeof((arr)[0]))

#endif /* _KERNEL_TYPES_H */
