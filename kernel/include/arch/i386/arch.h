/*
 * Atomical OS - i386 Architecture Header
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _ARCH_I386_H
#define _ARCH_I386_H

#include <kernel/types.h>

/* --- CPU Context --- */

typedef struct cpu_context {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t eip, cs, eflags, user_esp, user_ss;
} cpu_context_t;

/* --- GDT --- */

#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_CODE    0x18
#define GDT_USER_DATA    0x20
#define GDT_TSS          0x28

typedef struct PACKED gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
} gdt_entry_t;

typedef struct PACKED gdt_ptr {
    uint16_t limit;
    uint32_t base;
} gdt_ptr_t;

void gdt_init(void);

/* --- IDT --- */

#define IDT_ENTRIES 256

typedef struct PACKED idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} idt_entry_t;

typedef struct PACKED idt_ptr {
    uint16_t limit;
    uint32_t base;
} idt_ptr_t;

void idt_init(void);
void idt_set_gate(uint8_t vector, uint32_t handler, uint8_t type_attr);

/* --- Port I/O --- */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

/* --- Interrupt control --- */

static inline void cli(void) {
    __asm__ volatile("cli");
}

static inline void sti(void) {
    __asm__ volatile("sti");
}

static inline void i386_hlt(void) {
    __asm__ volatile("hlt");
}

#endif /* _ARCH_I386_H */
