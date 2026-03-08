/*
 * Atomical OS - Kernel Library Functions
 * String utilities, kprintf, panic.
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <hal/hal.h>

/* --- String functions --- */

size_t strlen(const char *s)
{
    size_t len = 0;
    while (s[len])
        len++;
    return len;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i])
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        if (s1[i] == '\0')
            return 0;
    }
    return 0;
}

char *strcpy(char *dst, const char *src)
{
    char *ret = dst;
    while ((*dst++ = *src++))
        ;
    return ret;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = '\0';
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memset(void *s, int c, size_t n)
{
    uint8_t *p = (uint8_t *)s;
    while (n--)
        *p++ = (uint8_t)c;
    return s;
}

void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dst;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return a[i] - b[i];
    }
    return 0;
}

/* --- Number to string conversion --- */

static void uint_to_str(uint64_t val, char *buf, int base, bool uppercase)
{
    static const char digits_lower[] = "0123456789abcdef";
    static const char digits_upper[] = "0123456789ABCDEF";
    const char *digits = uppercase ? digits_upper : digits_lower;

    char tmp[65];
    int i = 0;

    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0) {
            tmp[i++] = digits[val % base];
            val /= base;
        }
    }

    /* Reverse into buf */
    int j = 0;
    while (i > 0)
        buf[j++] = tmp[--i];
    buf[j] = '\0';
}

static void int_to_str(int64_t val, char *buf)
{
    if (val < 0) {
        *buf++ = '-';
        val = -val;
    }
    uint_to_str((uint64_t)val, buf, 10, false);
}

/* --- vkprintf: kernel printf core with va_list --- */

static void vkprintf(const char *fmt, __builtin_va_list args)
{
    char buf[128];

    while (*fmt) {
        if (*fmt != '%') {
            hal_serial_putc(*fmt++);
            continue;
        }

        fmt++;  /* skip '%' */

        /* Handle 'l' length modifiers */
        bool is_long = false;
        if (*fmt == 'l') {
            is_long = true;
            fmt++;
            if (*fmt == 'l')
                fmt++;  /* %llu etc. */
        }

        switch (*fmt) {
        case 's': {
            const char *s = __builtin_va_arg(args, const char *);
            if (!s) s = "(null)";
            hal_serial_puts(s);
            break;
        }
        case 'd':
        case 'i': {
            int64_t val;
            if (is_long)
                val = __builtin_va_arg(args, int64_t);
            else
                val = __builtin_va_arg(args, int);
            int_to_str(val, buf);
            hal_serial_puts(buf);
            break;
        }
        case 'u': {
            uint64_t val;
            if (is_long)
                val = __builtin_va_arg(args, uint64_t);
            else
                val = __builtin_va_arg(args, unsigned int);
            uint_to_str(val, buf, 10, false);
            hal_serial_puts(buf);
            break;
        }
        case 'x': {
            uint64_t val;
            if (is_long)
                val = __builtin_va_arg(args, uint64_t);
            else
                val = __builtin_va_arg(args, unsigned int);
            uint_to_str(val, buf, 16, false);
            hal_serial_puts(buf);
            break;
        }
        case 'X': {
            uint64_t val;
            if (is_long)
                val = __builtin_va_arg(args, uint64_t);
            else
                val = __builtin_va_arg(args, unsigned int);
            uint_to_str(val, buf, 16, true);
            hal_serial_puts(buf);
            break;
        }
        case 'p': {
            uintptr_t val = (uintptr_t)__builtin_va_arg(args, void *);
            hal_serial_puts("0x");
            uint_to_str(val, buf, 16, false);
            hal_serial_puts(buf);
            break;
        }
        case 'c': {
            char c = (char)__builtin_va_arg(args, int);
            hal_serial_putc(c);
            break;
        }
        case '%':
            hal_serial_putc('%');
            break;
        default:
            hal_serial_putc('%');
            hal_serial_putc(*fmt);
            break;
        }
        fmt++;
    }
}

/* --- kprintf: minimal kernel printf --- */

void kprintf(const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    vkprintf(fmt, args);
    __builtin_va_end(args);
}

/* --- klog: structured kernel logging --- */

void klog(log_level_t level, const char *subsystem, const char *fmt, ...)
{
    static const char *level_names[] = {
        "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
    };

    kprintf("[%s] %s: ", level_names[level], subsystem);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    vkprintf(fmt, args);
    __builtin_va_end(args);

    kprintf("\n");
}

/* --- panic: unrecoverable kernel error --- */

NORETURN void panic(const char *fmt, ...)
{
    hal_interrupts_disable();

    kprintf("\n\n!!! KERNEL PANIC !!!\n");
    kprintf("Reason: ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    vkprintf(fmt, args);
    __builtin_va_end(args);

    kprintf("\n\nSystem halted.\n");

    for (;;)
        hal_cpu_halt();
}

/* --- halt: stop the CPU --- */

NORETURN void halt(void)
{
    hal_interrupts_disable();
    for (;;)
        hal_cpu_halt();
}
