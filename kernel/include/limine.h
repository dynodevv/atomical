#ifndef ATOMICAL_LIMINE_H
#define ATOMICAL_LIMINE_H

#include <stdint.h>

#define LIMINE_FRAMEBUFFER_REQUEST {0xc7b1dd30df4c8b88ULL, 0x0a82e883a194f07bULL, 0x9d5827dcd881dd75ULL, 0xa3148604f6fab11bULL}

struct limine_framebuffer {
    void *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t reserved[7];
};

struct limine_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_framebuffer_response *response;
};

#endif
