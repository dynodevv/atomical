/*
 * Atomical OS - Kernel Main Entry Point
 * Architecture-independent kernel initialization.
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <kernel/limine.h>
#include <kernel/mm.h>
#include <kernel/vfs.h>
#include <hal/hal.h>

/* --- Limine Requests --- */

SECTION(".limine_requests")
static volatile struct limine_bootloader_info_request bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0,
};

SECTION(".limine_requests")
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
};

SECTION(".limine_requests")
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

SECTION(".limine_requests")
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
};

SECTION(".limine_requests")
static volatile struct limine_kernel_address_request kernel_addr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
};

SECTION(".limine_requests")
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0,
};

SECTION(".limine_requests")
static volatile struct limine_smp_request smp_request = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0,
    .flags = 0,
};

/* Limine requires these anchor markers */
SECTION(".limine_requests_start")
static volatile USED uint64_t limine_requests_start_marker[4] = {
    0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf,
    0x785c6ed015d3e316, 0x181e920a7852b9d9,
};

SECTION(".limine_requests_end")
static volatile USED uint64_t limine_requests_end_marker[2] = {
    0xadc0e0531bb10d03, 0x9572709f31764c62,
};

/* Global kernel state */
static hal_framebuffer_t kernel_fb;

/*
 * Draw a simple colored rectangle on the framebuffer.
 * Used during early boot to provide visual feedback.
 */
static void fb_fill_rect(hal_framebuffer_t *fb, uint32_t x, uint32_t y,
                          uint32_t w, uint32_t h, uint32_t color)
{
    for (uint32_t row = y; row < y + h && row < fb->height; row++) {
        uint32_t *pixel = (uint32_t *)((uint8_t *)fb->address + row * fb->pitch + x * (fb->bpp / 8));
        for (uint32_t col = 0; col < w && (x + col) < fb->width; col++) {
            pixel[col] = color;
        }
    }
}

/*
 * Display a boot splash: centered Atomical logo bar with gradient.
 */
static void boot_splash(hal_framebuffer_t *fb)
{
    /* Clear screen to dark background */
    fb_fill_rect(fb, 0, 0, fb->width, fb->height, 0x001A1A2E);

    /* Draw centered accent bar */
    uint32_t bar_w = fb->width / 3;
    uint32_t bar_h = 8;
    uint32_t bar_x = (fb->width - bar_w) / 2;
    uint32_t bar_y = fb->height / 2 - bar_h / 2;

    /* Gradient bar from blue to purple */
    for (uint32_t col = 0; col < bar_w; col++) {
        uint8_t r = (uint8_t)(60 + (col * 140) / bar_w);
        uint8_t g = (uint8_t)(80 - (col * 40) / bar_w);
        uint8_t b = (uint8_t)(220 - (col * 50) / bar_w);
        uint32_t color = (r << 16) | (g << 8) | b;
        for (uint32_t row = bar_y; row < bar_y + bar_h; row++) {
            uint32_t *pixel = (uint32_t *)((uint8_t *)fb->address + row * fb->pitch + (bar_x + col) * 4);
            *pixel = color;
        }
    }

    /* Small loading dots below */
    uint32_t dot_y = bar_y + bar_h + 20;
    for (int i = 0; i < 3; i++) {
        uint32_t dot_x = (fb->width / 2) - 20 + i * 20;
        fb_fill_rect(fb, dot_x, dot_y, 6, 6, 0x00FFFFFF);
    }
}

/*
 * kmain - Architecture-independent kernel entry.
 * Called by arch-specific boot code after minimal hardware init.
 */
void kmain(void)
{
    /* Step 1: Initialize serial console for early debug output */
    hal_serial_init();

    kprintf("\n");
    kprintf("==============================================\n");
    kprintf("  Atomical OS v%s\n", ATOMICAL_VERSION_STRING);
    kprintf("  MIT License - Copyright (c) 2026 Dyno\n");
    kprintf("==============================================\n\n");

    /* Log bootloader info */
    if (bootloader_info_request.response) {
        kprintf("[boot] Bootloader: %s %s\n",
                bootloader_info_request.response->name,
                bootloader_info_request.response->version);
    }

    /* Step 2: Initialize framebuffer and show boot splash */
    if (framebuffer_request.response &&
        framebuffer_request.response->framebuffer_count > 0) {
        struct limine_framebuffer *lfb = framebuffer_request.response->framebuffers[0];
        kernel_fb.address = lfb->address;
        kernel_fb.width   = (uint32_t)lfb->width;
        kernel_fb.height  = (uint32_t)lfb->height;
        kernel_fb.pitch   = (uint32_t)lfb->pitch;
        kernel_fb.bpp     = (uint8_t)lfb->bpp;
        kprintf("[boot] Framebuffer: %ux%u, %u bpp\n",
                kernel_fb.width, kernel_fb.height, kernel_fb.bpp);
        boot_splash(&kernel_fb);
    } else {
        kprintf("[boot] WARNING: No framebuffer available\n");
    }

    /* Step 3: Parse memory map and initialize physical memory manager */
    if (memmap_request.response) {
        kprintf("[boot] Memory map: %lu entries\n",
                memmap_request.response->entry_count);
        pmm_init(memmap_request.response->entries,
                 memmap_request.response->entry_count);
        kprintf("[mm]   Total: %lu MB, Free: %lu MB\n",
                pmm_get_total_memory() / (1024 * 1024),
                pmm_get_free_memory() / (1024 * 1024));
    } else {
        panic("No memory map from bootloader!");
    }

    /* Step 4: HHDM offset */
    if (hhdm_request.response) {
        kprintf("[boot] HHDM offset: 0x%lx\n", hhdm_request.response->offset);
    }

    /* Step 5: Initialize MMU / paging */
    kprintf("[mm]   Initializing virtual memory...\n");
    hal_mmu_init();

    /* Step 6: Initialize kernel heap */
    kprintf("[mm]   Initializing kernel heap...\n");
    heap_init();

    /* Step 7: Initialize interrupt subsystem */
    kprintf("[irq]  Initializing interrupt controller...\n");
    hal_interrupts_init();

    /* Step 8: Initialize timers */
    kprintf("[timer] Initializing system timer...\n");
    hal_timer_init(1000);  /* 1000 Hz tick rate */

    /* Step 9: Initialize VFS */
    kprintf("[vfs]  Initializing virtual file system...\n");
    vfs_init();

    /* Step 10: SMP initialization */
    if (smp_request.response) {
        kprintf("[smp]  CPU count: %lu (BSP LAPIC ID: %u)\n",
                smp_request.response->cpu_count,
                smp_request.response->bsp_lapic_id);
    }

    /* Step 11: Enable interrupts */
    hal_interrupts_enable();

    kprintf("\n[kernel] Atomical OS initialization complete.\n");
    kprintf("[kernel] Entering idle loop.\n\n");

    /* Idle loop */
    for (;;) {
        hal_cpu_idle();
    }
}
