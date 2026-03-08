/*
 * Atomical OS - x86_64 Architecture Header
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _ARCH_X86_64_H
#define _ARCH_X86_64_H

#include <kernel/types.h>

/* --- CPU Context (matches hal_context_switch layout in entry.S) --- */

typedef struct cpu_context {
    uint64_t r15;   /* offset  0 */
    uint64_t r14;   /* offset  8 */
    uint64_t r13;   /* offset 16 */
    uint64_t r12;   /* offset 24 */
    uint64_t rbp;   /* offset 32 */
    uint64_t rbx;   /* offset 40 */
    uint64_t rsp;   /* offset 48 */
    uint64_t rip;   /* offset 56 */
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
    uint64_t base;
} gdt_ptr_t;

void gdt_init(void);

/* --- IDT --- */

#define IDT_ENTRIES 256

typedef struct PACKED idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} idt_entry_t;

typedef struct PACKED idt_ptr {
    uint16_t limit;
    uint64_t base;
} idt_ptr_t;

void idt_init(void);
void idt_set_gate(uint8_t vector, uintptr_t handler, uint8_t ist, uint8_t type_attr);

/* --- Paging (4-level) --- */

/* PML4 -> PDPT -> PD -> PT */
#define PML4_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)   (((addr) >> 12) & 0x1FF)

#define PTE_PRESENT   BIT(0)
#define PTE_WRITABLE  BIT(1)
#define PTE_USER      BIT(2)
#define PTE_PWT       BIT(3)
#define PTE_PCD       BIT(4)
#define PTE_ACCESSED  BIT(5)
#define PTE_DIRTY     BIT(6)
#define PTE_HUGE      BIT(7)
#define PTE_GLOBAL    BIT(8)
#define PTE_NX        BIT(63)

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

typedef uint64_t pte_t;

typedef struct page_table {
    pte_t entries[512];
} ALIGNED(PAGE_SIZE) page_table_t;

void x86_64_paging_init(void);

/* --- Port I/O --- */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

/* --- MSR --- */

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ volatile("wrmsr" : : "a"((uint32_t)val), "d"((uint32_t)(val >> 32)), "c"(msr));
}

/* --- Control Registers --- */

static inline uint64_t read_cr0(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint64_t val) {
    __asm__ volatile("mov %0, %%cr0" : : "r"(val));
}

static inline uint64_t read_cr2(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(uint64_t val) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(val) : "memory");
}

static inline uint64_t read_cr4(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(uint64_t val) {
    __asm__ volatile("mov %0, %%cr4" : : "r"(val));
}

static inline void invlpg(uintptr_t addr) {
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

/* --- Interrupt control --- */

static inline void cli(void) {
    __asm__ volatile("cli");
}

static inline void sti(void) {
    __asm__ volatile("sti");
}

static inline void hlt(void) {
    __asm__ volatile("hlt");
}

static inline uint64_t read_rflags(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0" : "=r"(flags));
    return flags;
}

#endif /* _ARCH_X86_64_H */
