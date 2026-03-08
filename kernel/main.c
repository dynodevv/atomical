/*
 * Atomical OS - Kernel Main Entry Point
 * Architecture-independent kernel initialization.
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <kernel/limine.h>
#include <kernel/mm.h>
#include <kernel/vfs.h>
#include <kernel/sched.h>
#include <kernel/fbconsole.h>
#include <kernel/keyboard.h>
#include <kernel/ktest.h>
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

/* Forward declaration for shell command dispatcher */
static void shell_execute(char *line);

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
        if (kernel_fb.bpp == 32) {
            boot_splash(&kernel_fb);
        } else {
            kprintf("[boot] WARNING: Splash requires 32-bpp framebuffer, skipping\n");
        }
    } else {
        kprintf("[boot] WARNING: No framebuffer available\n");
    }

    /* Step 3: Store HHDM offset (must be before pmm_init and heap_init) */
    if (hhdm_request.response) {
        hhdm_offset = hhdm_request.response->offset;
        kprintf("[boot] HHDM offset: 0x%lx\n", hhdm_offset);
    } else {
        panic("No HHDM response from bootloader!");
    }

    /* Step 4: Parse memory map and initialize physical memory manager */
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

    /* Step 9: Initialize VFS (also registers and mounts RamFS) */
    kprintf("[vfs]  Initializing virtual file system...\n");
    vfs_init();

    /* Mount RamFS on /tmp for user-accessible in-memory storage */
    vfs_mount("none", "/tmp", "ramfs", NULL);

    /* Step 10: Initialize framebuffer console for text output */
    kprintf("[fb]   Initializing framebuffer console...\n");
    if (kernel_fb.bpp == 32) {
        fbcon_init(&kernel_fb);
        kprintf("[fb]   Console active: %ux%u chars\n",
                kernel_fb.width / 8, kernel_fb.height / 16);
    }

    /* Re-print boot banner now that console is active */
    kprintf("\n");
    kprintf("  Atomical OS v%s\n", ATOMICAL_VERSION_STRING);
    kprintf("  MIT License - Copyright (c) 2026 Dyno\n\n");

    /* Step 11: SMP initialization */
    if (smp_request.response) {
        kprintf("[smp]  CPU count: %lu (BSP LAPIC ID: %u)\n",
                smp_request.response->cpu_count,
                smp_request.response->bsp_lapic_id);
    }

    /* Step 12: Initialize keyboard driver */
    kprintf("[kbd]  Initializing keyboard driver...\n");
    keyboard_init();

    /* Step 13: Enable interrupts */
    hal_interrupts_enable();

    /* Step 14: Initialize scheduler */
    kprintf("[sched] Initializing scheduler...\n");
    sched_init();

    /* Step 15: Run kernel self-tests */
    ktest_results_t results = ktest_run_all();

    kprintf("[kernel] Atomical OS initialization complete.\n");
    if (results.failed > 0) {
        kprintf("[kernel] WARNING: %d test(s) failed!\n", results.failed);
    }

    /* Step 16: Interactive kernel shell */
    kprintf("\nType 'help' for available commands.\n");
    kprintf("atomical> ");

    /* Simple kernel shell: echo typed characters, handle backspace/enter */
    char line[256];
    int line_pos = 0;

    for (;;) {
        char c = keyboard_poll();
        if (!c) {
            hal_cpu_idle();
            continue;
        }

        if (c == '\n') {
            kprintf("\n");
            line[line_pos] = '\0';

            /* Process commands */
            if (line_pos > 0) {
                shell_execute(line);
            }

            line_pos = 0;
            kprintf("atomical> ");
        } else if (c == '\b') {
            if (line_pos > 0) {
                line_pos--;
                kprintf("\b");
            }
        } else if (c >= 32 && c < 127 && line_pos < 255) {
            line[line_pos++] = c;
            kprintf("%c", c);
        }
    }
}

/* ============================================================
 *  Shell command implementations
 * ============================================================ */

/*
 * Simple argument parser: split line into argv[0], argv[1], ...
 * Returns argc. Modifies the input string in-place.
 */
static int shell_parse_args(char *line, char *argv[], int max_args)
{
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;

        argv[argc++] = p;

        /* Advance to next whitespace or end */
        while (*p && *p != ' ' && *p != '\t')
            p++;

        if (*p) {
            *p = '\0';
            p++;
        }
    }

    return argc;
}

/* --- help --- */
static void cmd_help(void)
{
    kprintf("Available commands:\n");
    kprintf("  help          - Show this help\n");
    kprintf("  info          - Show system information\n");
    kprintf("  mem           - Show memory statistics\n");
    kprintf("  uptime        - Show system uptime\n");
    kprintf("  clear         - Clear the screen\n");
    kprintf("  color <hex>   - Set text color (e.g. color 00FF00)\n");
    kprintf("\n");
    kprintf("Process management:\n");
    kprintf("  ps            - List running tasks\n");
    kprintf("  spawn <name>  - Create a background task\n");
    kprintf("  kill <pid>    - Terminate a task by PID\n");
    kprintf("\n");
    kprintf("Filesystem:\n");
    kprintf("  ls [path]     - List directory contents\n");
    kprintf("  mkdir <path>  - Create a directory\n");
    kprintf("  touch <path>  - Create an empty file\n");
    kprintf("  echo <text> > <file> - Write text to a file\n");
    kprintf("  cat <path>    - Display file contents\n");
    kprintf("  rm <path>     - Remove a file\n");
    kprintf("\n");
    kprintf("Testing:\n");
    kprintf("  test          - Run all kernel self-tests\n");
    kprintf("  test strings  - Test string functions\n");
    kprintf("  test heap     - Test heap allocator\n");
    kprintf("  test pmm      - Test physical memory manager\n");
    kprintf("  test timer    - Test timer subsystem\n");
    kprintf("  test sched    - Test scheduler\n");
    kprintf("  test vfs      - Test VFS / RamFS\n");
}

/* --- info --- */
static void cmd_info(void)
{
    kprintf("Atomical OS v%s\n", ATOMICAL_VERSION_STRING);
    kprintf("Architecture: "
#if defined(__x86_64__)
            "x86_64"
#elif defined(__aarch64__)
            "ARM64"
#else
            "unknown"
#endif
            "\n");
    if (smp_request.response)
        kprintf("CPUs: %lu\n", smp_request.response->cpu_count);
    kprintf("Framebuffer: %ux%u, %u bpp\n",
            kernel_fb.width, kernel_fb.height, kernel_fb.bpp);
}

/* --- mem --- */
static void cmd_mem(void)
{
    kprintf("Total: %lu MB\n", pmm_get_total_memory() / (1024 * 1024));
    kprintf("Free:  %lu MB\n", pmm_get_free_memory() / (1024 * 1024));
    kprintf("Used:  %lu MB\n",
            (pmm_get_total_memory() - pmm_get_free_memory()) / (1024 * 1024));
}

/* --- uptime --- */
static void cmd_uptime(void)
{
    uint64_t ticks = hal_timer_get_ticks();
    uint64_t seconds = ticks / 1000;
    uint64_t minutes = seconds / 60;
    uint64_t hours   = minutes / 60;

    kprintf("Uptime: ");
    if (hours > 0)
        kprintf("%luh ", hours);
    if (minutes > 0)
        kprintf("%lum ", minutes % 60);
    kprintf("%lus (%lu ticks)\n", seconds % 60, ticks);
}

/* --- ps --- */
static void cmd_ps(void)
{
    kprintf("PID  STATE      NAME\n");
    kprintf("---  ---------  ----\n");

    for (pid_t i = 0; i < 64; i++) {
        task_t *t = find_task_by_pid(i);
        if (!t)
            continue;

        const char *state_str;
        switch (t->state) {
        case TASK_RUNNING:  state_str = "RUNNING";  break;
        case TASK_READY:    state_str = "READY";    break;
        case TASK_BLOCKED:  state_str = "BLOCKED";  break;
        case TASK_SLEEPING: state_str = "SLEEPING"; break;
        case TASK_ZOMBIE:   state_str = "ZOMBIE";   break;
        case TASK_DEAD:     state_str = "DEAD";     break;
        default:            state_str = "UNKNOWN";  break;
        }

        kprintf("%-4d %-9s  %s\n", t->pid, state_str, t->name);
    }
}

/* --- spawn --- */
static volatile int spawn_counter = 0;

static void spawned_task_entry(void *arg)
{
    int id = (int)(uintptr_t)arg;
    kprintf("[task] Background task #%d running (PID %d)\n",
            id, sched_get_current()->pid);

    /* Run for a few scheduler cycles then exit */
    for (int i = 0; i < 5; i++)
        sched_yield();

    kprintf("[task] Background task #%d finished\n", id);
    sched_exit(0);
}

static void cmd_spawn(int argc, char *argv[])
{
    const char *name = (argc > 1) ? argv[1] : "worker";
    int id = ++spawn_counter;

    task_t *t = sched_create_task(name, spawned_task_entry, (void *)(uintptr_t)id, 1);
    if (t) {
        kprintf("Spawned task '%s' with PID %d\n", name, t->pid);
    } else {
        kprintf("Failed to spawn task\n");
    }
}

/* --- kill --- */
static void cmd_kill(int argc, char *argv[])
{
    if (argc < 2) {
        kprintf("Usage: kill <pid>\n");
        return;
    }

    /* Simple atoi */
    int pid = 0;
    const char *s = argv[1];
    while (*s >= '0' && *s <= '9') {
        pid = pid * 10 + (*s - '0');
        s++;
    }

    if (pid == 0) {
        kprintf("Cannot kill the idle task (PID 0)\n");
        return;
    }

    task_t *t = find_task_by_pid((pid_t)pid);
    if (!t) {
        kprintf("No task with PID %d\n", pid);
        return;
    }

    kprintf("Killed task '%s' (PID %d)\n", t->name, t->pid);
    sched_destroy_task(t);
}

/* --- ls --- */
static void cmd_ls(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : "/tmp";

    struct dentry *dir = vfs_lookup(path);
    if (!dir) {
        kprintf("ls: cannot access '%s': No such directory\n", path);
        return;
    }
    if (dir->inode->type != VFS_DIRECTORY) {
        kprintf("ls: '%s': Not a directory\n", path);
        return;
    }

    struct dentry *child = dir->children;
    if (!child) {
        kprintf("(empty)\n");
        return;
    }

    while (child) {
        const char *type_str;
        if (child->inode->type == VFS_DIRECTORY)
            type_str = "dir ";
        else
            type_str = "file";

        kprintf("  %s  %6lu  %s\n", type_str, child->inode->size, child->name);
        child = child->next;
    }
}

/* --- mkdir --- */
static void cmd_mkdir(int argc, char *argv[])
{
    if (argc < 2) {
        kprintf("Usage: mkdir <path>\n");
        return;
    }

    int rc = vfs_mkdir(argv[1], 0755);
    if (rc != 0)
        kprintf("mkdir: cannot create directory '%s'\n", argv[1]);
}

/* --- touch --- */
static void cmd_touch(int argc, char *argv[])
{
    if (argc < 2) {
        kprintf("Usage: touch <path>\n");
        return;
    }

    struct file *f = vfs_open(argv[1], O_CREAT | O_RDWR, 0644);
    if (f) {
        vfs_close(f);
    } else {
        kprintf("touch: cannot create '%s'\n", argv[1]);
    }
}

/* --- echo (with redirection) --- */
static void cmd_echo(int argc, char *argv[])
{
    if (argc < 2) {
        kprintf("\n");
        return;
    }

    /* Look for '>' redirection operator */
    int redir_idx = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0) {
            redir_idx = i;
            break;
        }
    }

    if (redir_idx > 0 && redir_idx + 1 < argc) {
        /* Redirect to file */
        const char *filepath = argv[redir_idx + 1];
        struct file *f = vfs_open(filepath, O_CREAT | O_RDWR, 0644);
        if (!f) {
            kprintf("echo: cannot open '%s'\n", filepath);
            return;
        }

        /* Build the text from args before '>' */
        for (int i = 1; i < redir_idx; i++) {
            vfs_write(f, argv[i], strlen(argv[i]));
            if (i < redir_idx - 1)
                vfs_write(f, " ", 1);
        }

        vfs_close(f);
    } else {
        /* No redirection: just print to console */
        for (int i = 1; i < argc; i++) {
            kprintf("%s", argv[i]);
            if (i < argc - 1)
                kprintf(" ");
        }
        kprintf("\n");
    }
}

/* --- cat --- */
static void cmd_cat(int argc, char *argv[])
{
    if (argc < 2) {
        kprintf("Usage: cat <path>\n");
        return;
    }

    struct file *f = vfs_open(argv[1], O_RDONLY, 0);
    if (!f) {
        kprintf("cat: cannot open '%s': No such file\n", argv[1]);
        return;
    }

    char buf[256];
    ssize_t n;
    while ((n = vfs_read(f, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        kprintf("%s", buf);
    }
    kprintf("\n");

    vfs_close(f);
}

/* --- rm --- */
static void cmd_rm(int argc, char *argv[])
{
    if (argc < 2) {
        kprintf("Usage: rm <path>\n");
        return;
    }

    int rc = vfs_unlink(argv[1]);
    if (rc != 0)
        kprintf("rm: cannot remove '%s': No such file\n", argv[1]);
}

/* --- color --- */
static void cmd_color(int argc, char *argv[])
{
    if (argc < 2) {
        kprintf("Usage: color <RRGGBB>\n");
        kprintf("Example: color 00FF00 (green)\n");
        return;
    }

    /* Parse hex color */
    const char *hex = argv[1];
    uint32_t color = 0;
    for (int i = 0; hex[i] && i < 6; i++) {
        color <<= 4;
        char c = hex[i];
        if (c >= '0' && c <= '9')
            color |= (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f')
            color |= (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            color |= (uint32_t)(c - 'A' + 10);
    }

    fbcon_set_color(color, 0x001A1A2E);
    kprintf("Text color set to #%s\n", hex);
}

/* --- test --- */
static void cmd_test(int argc, char *argv[])
{
    ktest_results_t results;

    if (argc < 2) {
        results = ktest_run_all();
    } else if (strcmp(argv[1], "strings") == 0) {
        results = ktest_run_strings();
    } else if (strcmp(argv[1], "heap") == 0) {
        results = ktest_run_heap();
    } else if (strcmp(argv[1], "pmm") == 0) {
        results = ktest_run_pmm();
    } else if (strcmp(argv[1], "timer") == 0) {
        results = ktest_run_timer();
    } else if (strcmp(argv[1], "sched") == 0) {
        results = ktest_run_sched();
    } else if (strcmp(argv[1], "vfs") == 0) {
        results = ktest_run_vfs();
    } else {
        kprintf("Unknown test suite: %s\n", argv[1]);
        kprintf("Available: strings, heap, pmm, timer, sched, vfs\n");
        return;
    }

    if (results.failed > 0) {
        kprintf("WARNING: %d/%d tests failed!\n", results.failed, results.total);
    } else {
        kprintf("All %d tests passed.\n", results.total);
    }
}

/* --- Command dispatcher --- */
static void shell_execute(char *line)
{
    char *argv[16];
    int argc = shell_parse_args(line, argv, 16);
    if (argc == 0)
        return;

    const char *cmd = argv[0];

    if (strcmp(cmd, "help") == 0)
        cmd_help();
    else if (strcmp(cmd, "info") == 0)
        cmd_info();
    else if (strcmp(cmd, "mem") == 0)
        cmd_mem();
    else if (strcmp(cmd, "uptime") == 0)
        cmd_uptime();
    else if (strcmp(cmd, "clear") == 0)
        fbcon_clear();
    else if (strcmp(cmd, "color") == 0)
        cmd_color(argc, argv);
    else if (strcmp(cmd, "ps") == 0)
        cmd_ps();
    else if (strcmp(cmd, "spawn") == 0)
        cmd_spawn(argc, argv);
    else if (strcmp(cmd, "kill") == 0)
        cmd_kill(argc, argv);
    else if (strcmp(cmd, "ls") == 0)
        cmd_ls(argc, argv);
    else if (strcmp(cmd, "mkdir") == 0)
        cmd_mkdir(argc, argv);
    else if (strcmp(cmd, "touch") == 0)
        cmd_touch(argc, argv);
    else if (strcmp(cmd, "echo") == 0)
        cmd_echo(argc, argv);
    else if (strcmp(cmd, "cat") == 0)
        cmd_cat(argc, argv);
    else if (strcmp(cmd, "rm") == 0)
        cmd_rm(argc, argv);
    else if (strcmp(cmd, "test") == 0)
        cmd_test(argc, argv);
    else {
        kprintf("Unknown command: %s\n", cmd);
        kprintf("Type 'help' for available commands.\n");
    }
}
