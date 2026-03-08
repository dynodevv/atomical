/*
 * Atomical OS - Hardware Abstraction Layer Interface
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _HAL_HAL_H
#define _HAL_HAL_H

#include <kernel/types.h>

/*
 * The HAL provides a uniform interface for architecture-specific operations.
 * Each architecture (x86_64, ARM64, i386) implements these functions.
 */

/* --- Serial / Debug Output --- */
void hal_serial_init(void);
void hal_serial_putc(char c);
void hal_serial_puts(const char *s);

/* --- Interrupt Control --- */
void hal_interrupts_init(void);
void hal_interrupts_enable(void);
void hal_interrupts_disable(void);
bool hal_interrupts_enabled(void);

/* Interrupt handler callback type */
typedef void (*irq_handler_t)(uint32_t irq, void *context);
int  hal_irq_register(uint32_t irq, irq_handler_t handler, void *context);
void hal_irq_unregister(uint32_t irq);
void hal_irq_ack(uint32_t irq);
void hal_irq_unmask(uint32_t irq);
void hal_irq_mask(uint32_t irq);

/* --- Timer --- */
void     hal_timer_init(uint32_t frequency_hz);
uint64_t hal_timer_get_ticks(void);
uint64_t hal_timer_get_ns(void);
void     hal_timer_udelay(uint64_t microseconds);

/* --- MMU / Paging --- */

/* Opaque page table type - architecture provides the implementation */
typedef struct page_table page_table_t;

/* Page flags */
#define PAGE_PRESENT     BIT(0)
#define PAGE_WRITABLE    BIT(1)
#define PAGE_USER        BIT(2)
#define PAGE_WRITE_THRU  BIT(3)
#define PAGE_NO_CACHE    BIT(4)
#define PAGE_ACCESSED    BIT(5)
#define PAGE_DIRTY       BIT(6)
#define PAGE_HUGE        BIT(7)
#define PAGE_GLOBAL      BIT(8)
#define PAGE_NO_EXEC     BIT(63)

void          hal_mmu_init(void);
page_table_t *hal_mmu_create_address_space(void);
void          hal_mmu_destroy_address_space(page_table_t *pt);
void          hal_mmu_switch_address_space(page_table_t *pt);
int           hal_mmu_map_page(page_table_t *pt, uintptr_t virt, uintptr_t phys, uint64_t flags);
int           hal_mmu_unmap_page(page_table_t *pt, uintptr_t virt);
uintptr_t     hal_mmu_virt_to_phys(page_table_t *pt, uintptr_t virt);
void          hal_mmu_invalidate_page(uintptr_t virt);

/* --- CPU Control --- */
void     hal_cpu_halt(void);
void     hal_cpu_idle(void);
uint32_t hal_cpu_id(void);
uint64_t hal_cpu_read_timestamp(void);

/* --- SMP --- */
void     hal_smp_init(void);
uint32_t hal_smp_get_cpu_count(void);
void     hal_smp_start_cpu(uint32_t cpu_id, void (*entry)(void *), void *arg);

/* --- Context Switching --- */

/* Architecture-specific CPU context (saved registers) */
typedef struct cpu_context cpu_context_t;

void hal_context_switch(cpu_context_t *old_ctx, cpu_context_t *new_ctx);
void hal_context_init(cpu_context_t *ctx, void *stack_top, void (*entry)(void *), void *arg);
void hal_userspace_enter(uintptr_t entry, uintptr_t user_stack, uintptr_t arg);

/* --- Framebuffer --- */
typedef struct {
    void    *address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t  bpp;
} hal_framebuffer_t;

int  hal_framebuffer_init(hal_framebuffer_t *fb);
void hal_framebuffer_put_pixel(hal_framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t color);

#endif /* _HAL_HAL_H */
