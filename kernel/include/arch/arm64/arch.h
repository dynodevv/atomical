/*
 * Atomical OS - ARM64 Architecture Header
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _ARCH_ARM64_H
#define _ARCH_ARM64_H

#include <kernel/types.h>

/* --- CPU Context (callee-saved + special registers) --- */

typedef struct cpu_context {
    uint64_t x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    uint64_t fp;    /* x29 */
    uint64_t lr;    /* x30 */
    uint64_t sp;
    uint64_t elr;   /* Exception Link Register */
    uint64_t spsr;  /* Saved Program Status Register */
} cpu_context_t;

/* --- Exception frame pushed on interrupt/exception --- */

typedef struct PACKED exception_frame {
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7;
    uint64_t x8, x9, x10, x11, x12, x13, x14, x15;
    uint64_t x16, x17, x18, x19, x20, x21, x22, x23;
    uint64_t x24, x25, x26, x27, x28, x29, x30;
    uint64_t elr_el1;
    uint64_t spsr_el1;
    uint64_t sp_el0;
} exception_frame_t;

/* --- Paging (4-level: PGD -> PUD -> PMD -> PTE) --- */

#define PGD_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PUD_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define PMD_INDEX(addr) (((addr) >> 21) & 0x1FF)
#define PTE_INDEX(addr) (((addr) >> 12) & 0x1FF)

/* ARM64 page table descriptor bits */
#define ARM64_PTE_VALID     BIT(0)
#define ARM64_PTE_TABLE     BIT(1)   /* Table descriptor at levels 0-2 */
#define ARM64_PTE_PAGE      BIT(1)   /* Page descriptor at level 3 */
#define ARM64_PTE_AF        BIT(10)  /* Access Flag */
#define ARM64_PTE_SH_INNER  (3ULL << 8)
#define ARM64_PTE_AP_RW     (0ULL << 6)
#define ARM64_PTE_AP_RO     (2ULL << 6)
#define ARM64_PTE_AP_USER   (1ULL << 6)
#define ARM64_PTE_UXN       BIT(54)  /* Unprivileged eXecute Never */
#define ARM64_PTE_PXN       BIT(53)  /* Privileged eXecute Never */

/* Memory attribute indices for MAIR_EL1 */
#define ARM64_MATTR_DEVICE   0
#define ARM64_MATTR_NORMAL   1

#define ARM64_PTE_ADDR_MASK  0x0000FFFFFFFFF000ULL

typedef uint64_t pte_t;

typedef struct page_table {
    pte_t entries[512];
} ALIGNED(PAGE_SIZE) page_table_t;

void arm64_paging_init(void);

/* --- System Register Access --- */

#define SYSREG_READ(reg) ({ \
    uint64_t _val; \
    __asm__ volatile("mrs %0, " #reg : "=r"(_val)); \
    _val; \
})

#define SYSREG_WRITE(reg, val) do { \
    __asm__ volatile("msr " #reg ", %0" : : "r"((uint64_t)(val))); \
} while (0)

/* --- GICv3 (Generic Interrupt Controller) --- */

#define GICD_BASE   0x08000000  /* QEMU virt machine default */
#define GICR_BASE   0x080A0000

void gicv3_init(void);
void gicv3_enable_irq(uint32_t irq);
void gicv3_disable_irq(uint32_t irq);
void gicv3_send_eoi(uint32_t irq);
uint32_t gicv3_ack_irq(void);

/* --- PL011 UART --- */

#define PL011_BASE  0x09000000  /* QEMU virt machine default */

void pl011_init(void);
void pl011_putc(char c);
char pl011_getc(void);

/* --- ARM Generic Timer --- */

void arm_timer_init(uint32_t freq_hz);
void arm_timer_enable(void);
void arm_timer_disable(void);
uint64_t arm_timer_get_count(void);
uint64_t arm_timer_get_frequency(void);

/* --- Interrupt control --- */

static inline void arm64_enable_interrupts(void) {
    __asm__ volatile("msr daifclr, #0xf");
}

static inline void arm64_disable_interrupts(void) {
    __asm__ volatile("msr daifset, #0xf");
}

static inline uint64_t arm64_read_daif(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, daif" : "=r"(val));
    return val;
}

/* --- Cache / TLB maintenance --- */

static inline void arm64_isb(void) {
    __asm__ volatile("isb" ::: "memory");
}

static inline void arm64_dsb(void) {
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline void arm64_tlbi_all(void) {
    __asm__ volatile("tlbi vmalle1is" ::: "memory");
    arm64_dsb();
    arm64_isb();
}

static inline void arm64_tlbi_va(uintptr_t va) {
    va >>= 12;
    __asm__ volatile("tlbi vale1is, %0" : : "r"(va) : "memory");
    arm64_dsb();
    arm64_isb();
}

#endif /* _ARCH_ARM64_H */
