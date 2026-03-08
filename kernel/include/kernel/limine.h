/*
 * Atomical OS - Limine Boot Protocol Header
 * Based on the Limine Boot Protocol specification.
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _KERNEL_LIMINE_H
#define _KERNEL_LIMINE_H

#include <kernel/types.h>

/* Limine magic numbers */
#define LIMINE_COMMON_MAGIC_1 0xc7b1dd30df4c8b88
#define LIMINE_COMMON_MAGIC_2 0x0a82e883a194f07b

/* --- Request/Response ID macros --- */

#define LIMINE_BOOTLOADER_INFO_REQUEST \
    { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, 0xf55038d8e2a1202f, 0x279426fcf5f59740 }

#define LIMINE_FRAMEBUFFER_REQUEST \
    { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, 0x9d5827dcd881dd75, 0xa3148604f6fab11b }

#define LIMINE_MEMMAP_REQUEST \
    { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62 }

#define LIMINE_HHDM_REQUEST \
    { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, 0x48dcf1cb8ad2b852, 0x63984e959a98244b }

#define LIMINE_KERNEL_ADDRESS_REQUEST \
    { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, 0x71ba76863cc55f63, 0xb2644a48c516a487 }

#define LIMINE_RSDP_REQUEST \
    { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, 0xc5e77b6b397e7b43, 0x27637845accdcf3c }

#define LIMINE_SMP_REQUEST \
    { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, 0x95a67b819a1b857e, 0xa0b61b723b6a73e0 }

#define LIMINE_MODULE_REQUEST \
    { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, 0x3e7e279702be32af, 0xca1c4f3bd1280cee }

#define LIMINE_KERNEL_FILE_REQUEST \
    { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, 0xad97e90e83f1ed67, 0x31eb5d1c5ff23b69 }

/* --- Memory map entry types --- */
#define LIMINE_MEMMAP_USABLE                 0
#define LIMINE_MEMMAP_RESERVED               1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE       2
#define LIMINE_MEMMAP_ACPI_NVS               3
#define LIMINE_MEMMAP_BAD_MEMORY             4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_KERNEL_AND_MODULES     6
#define LIMINE_MEMMAP_FRAMEBUFFER            7

/* --- Data structures --- */

struct limine_uuid {
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint8_t  d[8];
};

struct limine_file {
    uint64_t revision;
    void    *address;
    uint64_t size;
    char    *path;
    char    *cmdline;
    uint32_t media_type;
    uint32_t unused;
    uint32_t tftp_ip;
    uint32_t tftp_port;
    uint32_t partition_index;
    uint32_t mbr_disk_id;
    struct limine_uuid gpt_disk_uuid;
    struct limine_uuid gpt_part_uuid;
    struct limine_uuid part_uuid;
};

/* --- Bootloader Info --- */

struct limine_bootloader_info_response {
    uint64_t revision;
    char    *name;
    char    *version;
};

struct limine_bootloader_info_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_bootloader_info_response *response;
};

/* --- Framebuffer --- */

struct limine_framebuffer {
    void    *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t  memory_model;
    uint8_t  red_mask_size;
    uint8_t  red_mask_shift;
    uint8_t  green_mask_size;
    uint8_t  green_mask_shift;
    uint8_t  blue_mask_size;
    uint8_t  blue_mask_shift;
    uint8_t  unused[7];
    uint64_t edid_size;
    void    *edid;
    /* Revision 1 */
    uint64_t mode_count;
    void   **modes;
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

/* --- Memory Map --- */

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response {
    uint64_t revision;
    uint64_t entry_count;
    struct limine_memmap_entry **entries;
};

struct limine_memmap_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_memmap_response *response;
};

/* --- HHDM (Higher Half Direct Map) --- */

struct limine_hhdm_response {
    uint64_t revision;
    uint64_t offset;
};

struct limine_hhdm_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_hhdm_response *response;
};

/* --- Kernel Address --- */

struct limine_kernel_address_response {
    uint64_t revision;
    uint64_t physical_base;
    uint64_t virtual_base;
};

struct limine_kernel_address_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_kernel_address_response *response;
};

/* --- RSDP --- */

struct limine_rsdp_response {
    uint64_t revision;
    void    *address;
};

struct limine_rsdp_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_rsdp_response *response;
};

/* --- SMP (Symmetric Multi-Processing) --- */

typedef void (*limine_goto_address)(struct limine_smp_info *);

struct limine_smp_info {
    uint32_t processor_id;
    uint32_t lapic_id;       /* x86_64 */
    uint64_t reserved;
    limine_goto_address goto_address;
    uint64_t extra_argument;
};

struct limine_smp_response {
    uint64_t revision;
    uint32_t flags;
    uint32_t bsp_lapic_id;   /* x86_64 */
    uint64_t cpu_count;
    struct limine_smp_info **cpus;
};

struct limine_smp_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_smp_response *response;
    uint64_t flags;
};

/* --- Module --- */

struct limine_module_response {
    uint64_t revision;
    uint64_t module_count;
    struct limine_file **modules;
};

struct limine_module_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_module_response *response;
};

/* --- Kernel File --- */

struct limine_kernel_file_response {
    uint64_t revision;
    struct limine_file *kernel_file;
};

struct limine_kernel_file_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_kernel_file_response *response;
};

#endif /* _KERNEL_LIMINE_H */
