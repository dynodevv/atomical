/*
 * Atomical OS - ARM64 MMU / 4-Level Paging
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <kernel/mm.h>
#include <kernel/sched.h>
#include <arch/arm64/arch.h>
#include <hal/hal.h>

void hal_mmu_init(void)
{
    /* Limine configures MMU with identity + higher-half mapping.
     * Log current TTBR and prepare for our own page tables. */
    uint64_t ttbr0 = SYSREG_READ(TTBR0_EL1);
    uint64_t ttbr1 = SYSREG_READ(TTBR1_EL1);
    kprintf("[mmu]  TTBR0_EL1: 0x%lx\n", ttbr0);
    kprintf("[mmu]  TTBR1_EL1: 0x%lx\n", ttbr1);
}

page_table_t *hal_mmu_create_address_space(void)
{
    uintptr_t frame = pmm_alloc_frame();
    if (!frame) return NULL;

    page_table_t *pgd = (page_table_t *)PHYS_TO_VIRT(frame);
    memset(pgd, 0, PAGE_SIZE);
    return pgd;
}

void hal_mmu_destroy_address_space(page_table_t *pt)
{
    pmm_free_frame(VIRT_TO_PHYS(pt));
}

void hal_mmu_switch_address_space(page_table_t *pt)
{
    SYSREG_WRITE(TTBR0_EL1, VIRT_TO_PHYS(pt));
    arm64_isb();
    arm64_tlbi_all();
}

int hal_mmu_map_page(page_table_t *pgd, uintptr_t virt, uintptr_t phys, uint64_t flags)
{
    uint64_t pgd_idx = PGD_INDEX(virt);
    uint64_t pud_idx = PUD_INDEX(virt);
    uint64_t pmd_idx = PMD_INDEX(virt);
    uint64_t pte_idx = PTE_INDEX(virt);

    /* Convert generic flags to ARM64 PTE flags */
    uint64_t arm_flags = ARM64_PTE_VALID | ARM64_PTE_AF | ARM64_PTE_SH_INNER;
    arm_flags |= ((uint64_t)ARM64_MATTR_NORMAL << 2);  /* Normal memory */

    if (flags & PAGE_USER)
        arm_flags |= ARM64_PTE_AP_USER;
    if (!(flags & PAGE_WRITABLE))
        arm_flags |= ARM64_PTE_AP_RO;
    if (flags & PAGE_NO_EXEC)
        arm_flags |= ARM64_PTE_UXN | ARM64_PTE_PXN;

    /* Walk / allocate levels */
    pte_t *entry = &pgd->entries[pgd_idx];
    if (!(*entry & ARM64_PTE_VALID)) {
        uintptr_t frame = pmm_alloc_frame();
        if (!frame) return -1;
        memset(PHYS_TO_VIRT(frame), 0, PAGE_SIZE);
        *entry = frame | ARM64_PTE_VALID | ARM64_PTE_TABLE;
    }

    page_table_t *pud = (page_table_t *)PHYS_TO_VIRT(*entry & ARM64_PTE_ADDR_MASK);
    entry = &pud->entries[pud_idx];
    if (!(*entry & ARM64_PTE_VALID)) {
        uintptr_t frame = pmm_alloc_frame();
        if (!frame) return -1;
        memset(PHYS_TO_VIRT(frame), 0, PAGE_SIZE);
        *entry = frame | ARM64_PTE_VALID | ARM64_PTE_TABLE;
    }

    page_table_t *pmd = (page_table_t *)PHYS_TO_VIRT(*entry & ARM64_PTE_ADDR_MASK);
    entry = &pmd->entries[pmd_idx];
    if (!(*entry & ARM64_PTE_VALID)) {
        uintptr_t frame = pmm_alloc_frame();
        if (!frame) return -1;
        memset(PHYS_TO_VIRT(frame), 0, PAGE_SIZE);
        *entry = frame | ARM64_PTE_VALID | ARM64_PTE_TABLE;
    }

    page_table_t *pt = (page_table_t *)PHYS_TO_VIRT(*entry & ARM64_PTE_ADDR_MASK);
    pt->entries[pte_idx] = phys | arm_flags | ARM64_PTE_PAGE;

    return 0;
}

int hal_mmu_unmap_page(page_table_t *pgd, uintptr_t virt)
{
    uint64_t pgd_idx = PGD_INDEX(virt);
    uint64_t pud_idx = PUD_INDEX(virt);
    uint64_t pmd_idx = PMD_INDEX(virt);
    uint64_t pte_idx = PTE_INDEX(virt);

    pte_t *entry = &pgd->entries[pgd_idx];
    if (!(*entry & ARM64_PTE_VALID)) return -1;

    page_table_t *pud = (page_table_t *)PHYS_TO_VIRT(*entry & ARM64_PTE_ADDR_MASK);
    entry = &pud->entries[pud_idx];
    if (!(*entry & ARM64_PTE_VALID)) return -1;

    page_table_t *pmd = (page_table_t *)PHYS_TO_VIRT(*entry & ARM64_PTE_ADDR_MASK);
    entry = &pmd->entries[pmd_idx];
    if (!(*entry & ARM64_PTE_VALID)) return -1;

    page_table_t *pt = (page_table_t *)PHYS_TO_VIRT(*entry & ARM64_PTE_ADDR_MASK);
    pt->entries[pte_idx] = 0;

    arm64_tlbi_va(virt);
    return 0;
}

uintptr_t hal_mmu_virt_to_phys(page_table_t *pgd, uintptr_t virt)
{
    uint64_t pgd_idx = PGD_INDEX(virt);
    uint64_t pud_idx = PUD_INDEX(virt);
    uint64_t pmd_idx = PMD_INDEX(virt);
    uint64_t pte_idx = PTE_INDEX(virt);

    pte_t *entry = &pgd->entries[pgd_idx];
    if (!(*entry & ARM64_PTE_VALID)) return 0;

    page_table_t *pud = (page_table_t *)PHYS_TO_VIRT(*entry & ARM64_PTE_ADDR_MASK);
    entry = &pud->entries[pud_idx];
    if (!(*entry & ARM64_PTE_VALID)) return 0;

    page_table_t *pmd = (page_table_t *)PHYS_TO_VIRT(*entry & ARM64_PTE_ADDR_MASK);
    entry = &pmd->entries[pmd_idx];
    if (!(*entry & ARM64_PTE_VALID)) return 0;

    page_table_t *pt = (page_table_t *)PHYS_TO_VIRT(*entry & ARM64_PTE_ADDR_MASK);
    if (!(pt->entries[pte_idx] & ARM64_PTE_VALID)) return 0;

    return (pt->entries[pte_idx] & ARM64_PTE_ADDR_MASK) | (virt & 0xFFF);
}

void hal_mmu_invalidate_page(uintptr_t virt)
{
    arm64_tlbi_va(virt);
}

/* --- Context Initialization (ARM64 stub) --- */

/* ARM64 task trampoline — called when a new task starts executing */
static void arm64_task_trampoline(void)
{
    /* On ARM64 context switch, x19-x28 are callee-saved and restored.
     * We use x19 = entry, x20 = arg (set by hal_context_init). */
    void (*entry)(void *);
    void *arg;
    __asm__ volatile("mov %0, x19" : "=r"(entry));
    __asm__ volatile("mov %0, x20" : "=r"(arg));
    entry(arg);
    sched_exit(0);
    for (;;) __asm__ volatile("wfi");
}

void hal_context_init(cpu_context_t *ctx, void *stack_top, void (*entry)(void *), void *arg)
{
    memset(ctx, 0, sizeof(cpu_context_t));
    ctx->x19 = (uint64_t)entry;
    ctx->x20 = (uint64_t)arg;
    ctx->sp   = (uint64_t)stack_top;
    ctx->lr   = (uint64_t)arm64_task_trampoline;
}

/* CPU HAL functions */

void hal_cpu_halt(void)
{
    __asm__ volatile("wfi");
}

void hal_cpu_idle(void)
{
    arm64_enable_interrupts();
    __asm__ volatile("wfi");
}

uint32_t hal_cpu_id(void)
{
    uint64_t mpidr = SYSREG_READ(MPIDR_EL1);
    return (uint32_t)(mpidr & 0xFF);
}

uint64_t hal_cpu_read_timestamp(void)
{
    return SYSREG_READ(CNTVCT_EL0);
}
