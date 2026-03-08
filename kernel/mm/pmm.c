/*
 * Atomical OS - Physical Memory Manager (PMM)
 * Bitmap-based frame allocator.
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <kernel/mm.h>
#include <kernel/limine.h>

/* Bitmap for tracking physical page frames */
#define BITMAP_MAX_PAGES (1024 * 1024 * 4)  /* Support up to 16 GB */

static uint64_t bitmap[BITMAP_MAX_PAGES / 64];
static uint64_t total_memory = 0;
static uint64_t free_memory = 0;
static uint64_t highest_page = 0;

/* Global HHDM offset — set by kmain before pmm_init */
uintptr_t hhdm_offset = 0;

/* Bitmap helpers */
static inline void bitmap_set(uint64_t page)
{
    bitmap[page / 64] |= (1ULL << (page % 64));
}

static inline void bitmap_clear(uint64_t page)
{
    bitmap[page / 64] &= ~(1ULL << (page % 64));
}

static inline bool bitmap_test(uint64_t page)
{
    return (bitmap[page / 64] >> (page % 64)) & 1;
}

void pmm_init(void *memmap_entries, uint64_t entry_count)
{
    struct limine_memmap_entry **entries = (struct limine_memmap_entry **)memmap_entries;

    /* Mark all pages as used initially */
    memset(bitmap, 0xFF, sizeof(bitmap));

    /* Walk memory map and mark usable regions as free */
    for (uint64_t i = 0; i < entry_count; i++) {
        struct limine_memmap_entry *entry = entries[i];

        /* Track total memory */
        uint64_t end = entry->base + entry->length;
        uint64_t end_page = end / PAGE_SIZE;
        if (end_page > BITMAP_MAX_PAGES)
            end_page = BITMAP_MAX_PAGES;
        if (end_page > highest_page)
            highest_page = end_page;

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            /* Mark these frames as free */
            uint64_t base_page = ALIGN_UP(entry->base, PAGE_SIZE) / PAGE_SIZE;
            uint64_t last_page = ALIGN_DOWN(entry->base + entry->length, PAGE_SIZE) / PAGE_SIZE;
            if (last_page > BITMAP_MAX_PAGES)
                last_page = BITMAP_MAX_PAGES;

            for (uint64_t p = base_page; p < last_page; p++) {
                bitmap_clear(p);
                free_memory += PAGE_SIZE;
            }
            total_memory += entry->length;
        } else {
            total_memory += entry->length;
        }
    }

    kprintf("[pmm]  Bitmap initialized: %lu pages tracked\n", highest_page);
}

uintptr_t pmm_alloc_frame(void)
{
    for (uint64_t i = 0; i < (highest_page + 63) / 64; i++) {
        if (bitmap[i] == 0xFFFFFFFFFFFFFFFF)
            continue;  /* All pages in this group are used */

        for (int bit = 0; bit < 64; bit++) {
            uint64_t page = i * 64 + bit;
            if (page >= highest_page)
                return 0;
            if (!bitmap_test(page)) {
                bitmap_set(page);
                free_memory -= PAGE_SIZE;
                uintptr_t addr = page * PAGE_SIZE;
                /* Note: This assumes identity mapping or HHDM offset is active.
                 * Frame zeroing should use the proper virtual address once
                 * the HHDM offset is stored globally after MMU init. */
                return addr;
            }
        }
    }
    return 0;  /* Out of memory */
}

void pmm_free_frame(uintptr_t frame)
{
    uint64_t page = frame / PAGE_SIZE;
    if (page < highest_page && bitmap_test(page)) {
        bitmap_clear(page);
        free_memory += PAGE_SIZE;
    }
}

uintptr_t pmm_alloc_frames(size_t count)
{
    /* Simple first-fit contiguous allocation */
    uint64_t start = 0;
    uint64_t found = 0;

    for (uint64_t page = 0; page < highest_page; page++) {
        if (bitmap_test(page)) {
            start = page + 1;
            found = 0;
        } else {
            found++;
            if (found == count) {
                /* Allocate all frames */
                for (uint64_t p = start; p < start + count; p++) {
                    bitmap_set(p);
                    free_memory -= PAGE_SIZE;
                }
                return start * PAGE_SIZE;
            }
        }
    }
    return 0;
}

void pmm_free_frames(uintptr_t frame, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        pmm_free_frame(frame + i * PAGE_SIZE);
    }
}

uint64_t pmm_get_total_memory(void)
{
    return total_memory;
}

uint64_t pmm_get_free_memory(void)
{
    return free_memory;
}
