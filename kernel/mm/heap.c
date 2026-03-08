/*
 * Atomical OS - Kernel Heap Allocator
 * Simple linked-list heap for early kernel use.
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <kernel/mm.h>

/* Heap block header */
typedef struct heap_block {
    size_t              size;
    bool                free;
    struct heap_block  *next;
    uint64_t            magic;  /* For corruption detection */
} heap_block_t;

#define HEAP_MAGIC      0xDEADBEEFCAFEBABE
#define HEAP_INITIAL_PAGES  64  /* 256 KB initial heap */
#define HEAP_BLOCK_HDR_SIZE sizeof(heap_block_t)

static heap_block_t *heap_start = NULL;
static size_t        heap_total = 0;

void heap_init(void)
{
    size_t heap_size = HEAP_INITIAL_PAGES * PAGE_SIZE;
    uintptr_t heap_base = pmm_alloc_frames(HEAP_INITIAL_PAGES);
    if (!heap_base) {
        panic("Failed to allocate kernel heap!");
    }

    heap_start = (heap_block_t *)heap_base;
    heap_start->size = heap_size - HEAP_BLOCK_HDR_SIZE;
    heap_start->free = true;
    heap_start->next = NULL;
    heap_start->magic = HEAP_MAGIC;

    heap_total = heap_size;

    kprintf("[heap] Initialized: %lu KB at %p\n", heap_size / 1024, (void *)heap_base);
}

void *kmalloc(size_t size)
{
    if (!size)
        return NULL;

    /* Align to 16 bytes */
    size = ALIGN_UP(size, 16);

    heap_block_t *block = heap_start;
    while (block) {
        if (block->magic != HEAP_MAGIC)
            panic("Heap corruption detected!");

        if (block->free && block->size >= size) {
            /* Split block if there's enough remaining space */
            if (block->size >= size + HEAP_BLOCK_HDR_SIZE + 32) {
                heap_block_t *new_block = (heap_block_t *)((uint8_t *)block + HEAP_BLOCK_HDR_SIZE + size);
                new_block->size = block->size - size - HEAP_BLOCK_HDR_SIZE;
                new_block->free = true;
                new_block->next = block->next;
                new_block->magic = HEAP_MAGIC;
                block->size = size;
                block->next = new_block;
            }
            block->free = false;
            return (void *)((uint8_t *)block + HEAP_BLOCK_HDR_SIZE);
        }
        block = block->next;
    }

    /* TODO: Expand heap by allocating more frames */
    return NULL;
}

void *kzalloc(size_t size)
{
    void *ptr = kmalloc(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - HEAP_BLOCK_HDR_SIZE);

    if (block->magic != HEAP_MAGIC)
        panic("kfree: invalid or corrupted block!");

    block->free = true;

    /* Coalesce with next block if free */
    if (block->next && block->next->free) {
        block->size += HEAP_BLOCK_HDR_SIZE + block->next->size;
        block->next = block->next->next;
    }
}

void *krealloc(void *ptr, size_t new_size)
{
    if (!ptr)
        return kmalloc(new_size);

    if (!new_size) {
        kfree(ptr);
        return NULL;
    }

    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - HEAP_BLOCK_HDR_SIZE);
    if (block->size >= new_size)
        return ptr;

    void *new_ptr = kmalloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        kfree(ptr);
    }
    return new_ptr;
}
