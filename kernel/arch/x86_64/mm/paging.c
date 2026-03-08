/*
 * Atomical OS - x86_64 4-Level Paging
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <kernel/mm.h>
#include <arch/x86_64/arch.h>
#include <hal/hal.h>

void hal_mmu_init(void)
{
    /* Limine sets up identity + higher-half mapping.
     * We note CR3 and prepare for our own page tables. */
    uint64_t cr3 = read_cr3();
    kprintf("[mmu]  Current CR3: 0x%lx\n", cr3);

    /* Enable NX bit if supported */
    uint64_t efer = rdmsr(0xC0000080);
    efer |= BIT(11);  /* NXE bit */
    wrmsr(0xC0000080, efer);
    kprintf("[mmu]  NX bit enabled\n");
}

page_table_t *hal_mmu_create_address_space(void)
{
    uintptr_t frame = pmm_alloc_frame();
    if (!frame) return NULL;

    page_table_t *pml4 = (page_table_t *)frame;
    memset(pml4, 0, PAGE_SIZE);

    /* Copy kernel mappings (upper half) from current PML4 */
    page_table_t *current = (page_table_t *)(read_cr3() & PTE_ADDR_MASK);
    for (int i = 256; i < 512; i++) {
        pml4->entries[i] = current->entries[i];
    }

    return pml4;
}

void hal_mmu_destroy_address_space(page_table_t *pt)
{
    /* Free user-space page tables (indices 0-255) */
    /* TODO: Recursively free page table tree */
    pmm_free_frame((uintptr_t)pt);
}

void hal_mmu_switch_address_space(page_table_t *pt)
{
    write_cr3((uint64_t)pt);
}

int hal_mmu_map_page(page_table_t *pml4, uintptr_t virt, uintptr_t phys, uint64_t flags)
{
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx   = PD_INDEX(virt);
    uint64_t pt_idx   = PT_INDEX(virt);

    /* Walk / allocate page table levels */
    pte_t *entry;

    /* PML4 -> PDPT */
    entry = &pml4->entries[pml4_idx];
    if (!(*entry & PTE_PRESENT)) {
        uintptr_t frame = pmm_alloc_frame();
        if (!frame) return -1;
        *entry = frame | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    }

    page_table_t *pdpt = (page_table_t *)(*entry & PTE_ADDR_MASK);

    /* PDPT -> PD */
    entry = &pdpt->entries[pdpt_idx];
    if (!(*entry & PTE_PRESENT)) {
        uintptr_t frame = pmm_alloc_frame();
        if (!frame) return -1;
        *entry = frame | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    }

    page_table_t *pd = (page_table_t *)(*entry & PTE_ADDR_MASK);

    /* PD -> PT */
    entry = &pd->entries[pd_idx];
    if (!(*entry & PTE_PRESENT)) {
        uintptr_t frame = pmm_alloc_frame();
        if (!frame) return -1;
        *entry = frame | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    }

    page_table_t *pt = (page_table_t *)(*entry & PTE_ADDR_MASK);

    /* PT -> Physical page */
    pt->entries[pt_idx] = phys | flags | PTE_PRESENT;

    return 0;
}

int hal_mmu_unmap_page(page_table_t *pml4, uintptr_t virt)
{
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx   = PD_INDEX(virt);
    uint64_t pt_idx   = PT_INDEX(virt);

    pte_t *entry = &pml4->entries[pml4_idx];
    if (!(*entry & PTE_PRESENT)) return -1;

    page_table_t *pdpt = (page_table_t *)(*entry & PTE_ADDR_MASK);
    entry = &pdpt->entries[pdpt_idx];
    if (!(*entry & PTE_PRESENT)) return -1;

    page_table_t *pd = (page_table_t *)(*entry & PTE_ADDR_MASK);
    entry = &pd->entries[pd_idx];
    if (!(*entry & PTE_PRESENT)) return -1;

    page_table_t *pt = (page_table_t *)(*entry & PTE_ADDR_MASK);
    pt->entries[pt_idx] = 0;

    invlpg(virt);
    return 0;
}

uintptr_t hal_mmu_virt_to_phys(page_table_t *pml4, uintptr_t virt)
{
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx   = PD_INDEX(virt);
    uint64_t pt_idx   = PT_INDEX(virt);

    pte_t *entry = &pml4->entries[pml4_idx];
    if (!(*entry & PTE_PRESENT)) return 0;

    page_table_t *pdpt = (page_table_t *)(*entry & PTE_ADDR_MASK);
    entry = &pdpt->entries[pdpt_idx];
    if (!(*entry & PTE_PRESENT)) return 0;

    page_table_t *pd = (page_table_t *)(*entry & PTE_ADDR_MASK);
    entry = &pd->entries[pd_idx];
    if (!(*entry & PTE_PRESENT)) return 0;

    page_table_t *pt = (page_table_t *)(*entry & PTE_ADDR_MASK);
    if (!(pt->entries[pt_idx] & PTE_PRESENT)) return 0;

    return (pt->entries[pt_idx] & PTE_ADDR_MASK) | (virt & 0xFFF);
}

void hal_mmu_invalidate_page(uintptr_t virt)
{
    invlpg(virt);
}

/* CPU HAL functions */

void hal_cpu_halt(void)
{
    hlt();
}

void hal_cpu_idle(void)
{
    sti();
    hlt();
}

uint32_t hal_cpu_id(void)
{
    /* Read LAPIC ID (simplified) */
    return 0; /* TODO: Read from APIC */
}

uint64_t hal_cpu_read_timestamp(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
