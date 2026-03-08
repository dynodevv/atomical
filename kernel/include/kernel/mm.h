/*
 * Atomical OS - Memory Management Header
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _KERNEL_MM_H
#define _KERNEL_MM_H

#include <kernel/types.h>

/* --- Physical Frame Allocator --- */

void      pmm_init(void *memmap, uint64_t entry_count);
uintptr_t pmm_alloc_frame(void);
void      pmm_free_frame(uintptr_t frame);
uintptr_t pmm_alloc_frames(size_t count);
void      pmm_free_frames(uintptr_t frame, size_t count);
uint64_t  pmm_get_total_memory(void);
uint64_t  pmm_get_free_memory(void);

/* --- Kernel Heap Allocator --- */

void  heap_init(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void *krealloc(void *ptr, size_t new_size);
void  kfree(void *ptr);

/* --- Slab Allocator --- */

typedef struct slab_cache slab_cache_t;

slab_cache_t *slab_cache_create(const char *name, size_t obj_size, size_t align);
void          slab_cache_destroy(slab_cache_t *cache);
void         *slab_alloc(slab_cache_t *cache);
void          slab_free(slab_cache_t *cache, void *obj);

/* --- Virtual Memory Areas --- */

#define VMA_READ    BIT(0)
#define VMA_WRITE   BIT(1)
#define VMA_EXEC    BIT(2)
#define VMA_USER    BIT(3)
#define VMA_SHARED  BIT(4)

typedef struct vma {
    uintptr_t   start;
    uintptr_t   end;
    uint64_t    flags;
    struct vma *next;
} vma_t;

typedef struct {
    vma_t       *vma_list;
    void        *page_table;     /* Architecture-specific page table */
    uint64_t     total_mapped;
} mm_struct_t;

mm_struct_t *mm_create(void);
void         mm_destroy(mm_struct_t *mm);
vma_t       *mm_mmap(mm_struct_t *mm, uintptr_t addr, size_t len, uint64_t flags);
int          mm_munmap(mm_struct_t *mm, uintptr_t addr, size_t len);
int          mm_handle_page_fault(mm_struct_t *mm, uintptr_t fault_addr, uint64_t error_code);

#endif /* _KERNEL_MM_H */
