# Atomical OS

<p align="center">
  <strong>A modern, multi-architecture Unix-like operating system built from scratch.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License: MIT">
  <img src="https://img.shields.io/badge/arch-x86__64%20|%20ARM64%20|%20i386-green.svg" alt="Architectures">
  <img src="https://img.shields.io/badge/bootloader-Limine-orange.svg" alt="Bootloader: Limine">
</p>

---

## Overview

Atomical is a Unix-like operating system designed and built entirely from scratch. It features a monolithic kernel with a strict Hardware Abstraction Layer (HAL), full graphical desktop environment, and a suite of built-in applications — targeting both physical hardware and virtual machines (optimized for QEMU).

### Key Features

- **Multi-Architecture Support:** x86_64 (UEFI/BIOS), ARM64/AArch64 (UEFI), i386 (BIOS)
- **Modern Kernel:** Preemptive multitasking, SMP support, 4-level paging, NX protections, ASLR
- **Hardware Abstraction Layer:** Clean separation between architecture-specific and generic code
- **Virtual File System:** Unified VFS with RamFS and EXT4 support
- **Custom TCP/IP Stack:** Full networking with Ethernet, ARP, IP, ICMP, UDP, TCP, DNS
- **Graphical Desktop:** Double-buffered compositor with macOS-style window management
- **Built-in Applications:** Console, file manager, text editor, image viewer, media player, calculator, settings, and more
- **Boot Experience:** Seamless graphical boot with animation via Limine bootloader

---

## Architecture

### System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                     User Applications                          │
│  Console │ Files │ Notes │ Viewer │ Player │ Settings │ ...    │
├─────────────────────────────────────────────────────────────────┤
│                   GUI Compositor / Window Server                │
│            Double-buffered │ Alpha blending │ Animations        │
├─────────────────────────────────────────────────────────────────┤
│                      System Call Interface                      │
│              sys_execve │ sys_read │ sys_write │ ...            │
╠═════════════════════════════════════════════════════════════════╣
│                         KERNEL SPACE                           │
├─────────────┬──────────────┬──────────────┬─────────────────────┤
│   Scheduler │    Memory    │     VFS      │   Network Stack    │
│  CFS / O(1) │  PMM + Heap  │ RamFS │ EXT4 │  TCP/IP + DNS     │
├─────────────┴──────────────┴──────────────┴─────────────────────┤
│               Hardware Abstraction Layer (HAL)                  │
│    Serial │ Interrupts │ Timer │ MMU │ Framebuffer │ CPU       │
├─────────────────────────────────────────────────────────────────┤
│              Architecture-Specific Implementations              │
│    x86_64 (APIC/PIT/UART)  │  ARM64 (GICv3/GenTimer/PL011)   │
├─────────────────────────────────────────────────────────────────┤
│                        Hardware                                │
└─────────────────────────────────────────────────────────────────┘
```

### Directory Structure

```
atomical/
├── kernel/                      # Kernel source code
│   ├── arch/                    # Architecture-specific code
│   │   ├── x86_64/
│   │   │   ├── boot/entry.S     # x86_64 entry point + ISR stubs
│   │   │   ├── mm/paging.c      # 4-level paging (PML4)
│   │   │   ├── interrupts/idt.c # IDT + APIC interrupt handling
│   │   │   ├── drivers/         # Serial (16550 UART), PIT timer
│   │   │   └── linker.ld        # x86_64 kernel linker script
│   │   ├── arm64/
│   │   │   ├── boot/entry.S     # ARM64 entry + exception vectors
│   │   │   ├── mm/paging.c      # 4-level paging (PGD/PUD/PMD/PTE)
│   │   │   ├── interrupts/gic.c # GICv3 interrupt controller
│   │   │   ├── drivers/         # PL011 UART, ARM Generic Timer
│   │   │   └── linker.ld        # ARM64 kernel linker script
│   │   └── i386/                # i386 support (scaffolding)
│   ├── include/
│   │   ├── kernel/              # Core kernel headers
│   │   │   ├── kernel.h         # Main kernel API
│   │   │   ├── types.h          # Standard type definitions
│   │   │   ├── limine.h         # Limine boot protocol
│   │   │   ├── mm.h             # Memory management
│   │   │   ├── sched.h          # Scheduler / process management
│   │   │   └── vfs.h            # Virtual file system
│   │   ├── hal/hal.h            # Hardware Abstraction Layer API
│   │   └── arch/                # Architecture-specific headers
│   ├── hal/                     # HAL shared implementations
│   ├── mm/                      # Memory management
│   │   ├── pmm.c               # Physical frame allocator (bitmap)
│   │   └── heap.c              # Kernel heap allocator
│   ├── sched/                   # Scheduler
│   ├── syscall/                 # System call handlers
│   ├── lib/klib.c              # Kernel library (kprintf, string ops)
│   └── main.c                  # Architecture-independent kernel entry
├── fs/                          # File systems
│   ├── vfs/vfs.c               # Virtual file system core
│   ├── ramfs/ramfs.c           # RAM-based filesystem
│   └── ext4/                    # EXT4 implementation (planned)
├── drivers/                     # Device drivers
│   ├── input/                   # Keyboard, mouse (Virtio tablet)
│   ├── display/                 # Framebuffer, VGA, Bochs graphics
│   ├── audio/                   # Intel HDA audio driver
│   ├── net/                     # Virtio-Net driver
│   ├── serial/                  # Serial console drivers
│   └── block/                   # Block device drivers
├── net/                         # Networking
│   ├── stack/                   # TCP/IP stack (Ethernet, ARP, IP, TCP, UDP, ICMP)
│   └── dns/                     # DNS resolver
├── gui/                         # Graphical environment
│   ├── compositor/              # Window server + compositor
│   ├── widgets/                 # UI widget library
│   ├── themes/                  # Dark/Light themes, accent colors
│   └── apps/                    # Built-in applications
│       ├── console/             # Terminal emulator
│       ├── files/               # File manager
│       ├── notes/               # Text editor
│       ├── viewer/              # Image viewer (JPEG/PNG)
│       ├── player/              # Media player (MP3)
│       ├── processes/           # Task manager
│       ├── calculator/          # Calculator
│       ├── clock/               # Clock / Stopwatch / Timer
│       ├── settings/            # System settings
│       └── installer/           # System installer
├── userspace/                   # Userspace components
│   ├── libc/                    # Minimal C library
│   ├── shell/                   # Bash-like shell
│   └── init/                    # Init process (PID 1)
├── assets/                      # Static resources
│   ├── wallpapers/              # Stock wallpaper images
│   ├── fonts/                   # System fonts
│   └── icons/                   # Application and file icons
├── tools/                       # Build and development tools
├── iso/boot/limine/             # Limine bootloader configuration
│   └── limine.conf              # Boot menu config (hidden menu)
├── .github/workflows/build.yml  # CI/CD pipeline
├── Makefile                     # Multi-arch build system
├── LICENSE                      # MIT License
└── README.md                   # This file
```

---

## Building

### Prerequisites

| Tool | Purpose |
|------|---------|
| `x86_64-elf-gcc` | Cross compiler for x86_64 target |
| `aarch64-elf-gcc` | Cross compiler for ARM64 target |
| `nasm` | Assembler (optional, for raw x86 assembly) |
| `xorriso` | ISO image creation |
| `mtools` | FAT filesystem manipulation for UEFI |
| `qemu-system-x86_64` | x86_64 emulation |
| `qemu-system-aarch64` | ARM64 emulation |
| `git` | For cloning Limine bootloader |

### Quick Start

```bash
# Clone the repository
git clone https://github.com/dynodevv/atomical.git
cd atomical

# Clone Limine bootloader (required for ISO builds)
git clone --depth=1 --branch=v8.x-binary https://github.com/limine-bootloader/limine.git
cd limine && make && cd ..

# Build the kernel for x86_64
make ARCH=x86_64 kernel

# Build a bootable ISO
make ARCH=x86_64 iso

# Run in QEMU
make ARCH=x86_64 run
```

### Building for ARM64

```bash
make ARCH=arm64 kernel
make ARCH=arm64 iso
make ARCH=arm64 run
```

### Build Targets

| Target | Description |
|--------|-------------|
| `make kernel` | Compile the kernel ELF binary |
| `make iso` | Build a bootable ISO image |
| `make run` | Build ISO and launch in QEMU |
| `make clean` | Remove all build artifacts |

### Configuration

Set the target architecture via the `ARCH` environment variable:

- `ARCH=x86_64` — AMD64 / Intel 64-bit (default)
- `ARCH=arm64` — AArch64 / ARM 64-bit
- `ARCH=i386` — Intel 32-bit (planned)

---

## Kernel Architecture

### Boot Process

1. **Limine Bootloader** loads the kernel ELF from the ISO
2. Boot menu is hidden — system boots directly into the kernel
3. **`_start`** (architecture-specific assembly): Sets up initial environment
4. **`kmain`** (C): Architecture-independent initialization sequence:
   - Initialize serial console for debug output
   - Parse Limine boot information (framebuffer, memory map, SMP)
   - Display graphical boot splash on framebuffer
   - Initialize Physical Memory Manager (bitmap-based frame allocator)
   - Initialize MMU / virtual memory (4-level paging)
   - Initialize kernel heap allocator
   - Initialize interrupt subsystem (IDT/APIC or GICv3)
   - Initialize system timer (PIT or ARM Generic Timer)
   - Initialize Virtual File System
   - Discover and prepare secondary CPUs (SMP)
   - Enable interrupts and enter idle loop

### Hardware Abstraction Layer (HAL)

The HAL (`kernel/include/hal/hal.h`) provides a uniform C API for all hardware operations:

```c
// Serial output
void hal_serial_init(void);
void hal_serial_putc(char c);

// Interrupt control
void hal_interrupts_init(void);
void hal_interrupts_enable(void);
void hal_interrupts_disable(void);
int  hal_irq_register(uint32_t irq, irq_handler_t handler, void *context);

// MMU / Paging
void      hal_mmu_init(void);
int       hal_mmu_map_page(page_table_t *pt, uintptr_t virt, uintptr_t phys, uint64_t flags);
uintptr_t hal_mmu_virt_to_phys(page_table_t *pt, uintptr_t virt);

// Timer
void     hal_timer_init(uint32_t frequency_hz);
uint64_t hal_timer_get_ticks(void);

// Context switching
void hal_context_switch(cpu_context_t *old, cpu_context_t *new);
void hal_userspace_enter(uintptr_t entry, uintptr_t user_stack, uintptr_t arg);
```

Each architecture implements these functions using its native hardware:

| HAL Function | x86_64 | ARM64 |
|---|---|---|
| Serial | 16550 UART (COM1) | PL011 UART |
| Interrupts | APIC / IOAPIC | GICv3 |
| Timer | PIT + LAPIC Timer | ARM Generic Timer |
| Paging | CR3 / PML4 | TTBR / PGD |
| Userspace | `iretq` | `eret` |

### Memory Management

- **Physical Memory Manager (PMM):** Bitmap-based frame allocator tracking all physical pages
- **Kernel Heap:** Linked-list allocator with block splitting and coalescing (`kmalloc`/`kfree`)
- **Virtual Memory:** 4-level page tables with NX bit protection, demand paging support
- **Slab Allocator:** Cache-based allocator for frequently-used kernel objects (planned)

### Process Management

- **Preemptive Multitasking:** Timer-driven scheduling with CFS-equivalent virtual runtime tracking
- **ELF Loading:** Full `sys_execve` with ELF validation, section parsing, `argc/argv/envp` setup
- **Context Switching:** Architecture-specific register save/restore with secure userspace transitions
- **SMP:** Boot on CPU 0, discover and spin up secondary CPUs

### Virtual File System (VFS)

The VFS provides a unified interface using standard Unix abstractions:

- `struct inode` — File/directory metadata
- `struct file` — Open file descriptor
- `struct dentry` — Directory entry (name → inode mapping)
- `struct superblock` — Mounted filesystem instance

Supported filesystems:
- **RamFS** — Memory-based filesystem for `/tmp` and initramfs
- **EXT4** — Full read/write support with block/inode bitmaps (planned)

---

## Security

- **NX Bit:** No-execute protection enforced in page tables
- **ASLR:** Address Space Layout Randomization for kernel and userspace
- **Login Manager:** Secure password hashing during installation
- **Media Sandbox:** Fault-isolated execution for media decoders (crash prevention)
- **Register Clearing:** All general-purpose registers zeroed before entering userspace

---

## Development Roadmap

### Phase 1: Core Kernel Foundation ✅
- [x] Project structure and build system
- [x] Limine boot protocol integration
- [x] x86_64 and ARM64 boot entry points
- [x] Serial console output (16550 UART / PL011)
- [x] Physical memory manager (bitmap frame allocator)
- [x] Kernel heap allocator
- [x] 4-level paging (x86_64 + ARM64)
- [x] Interrupt handling (IDT + ISR stubs / GICv3)
- [x] System timer (PIT / ARM Generic Timer)
- [x] VFS core + RamFS
- [x] Hardware Abstraction Layer
- [x] CI/CD pipeline (GitHub Actions)

### Phase 2: Process Management & Userspace
- [ ] Scheduler implementation (CFS)
- [ ] ELF binary loader
- [ ] `sys_execve` system call
- [ ] User stack initialization
- [ ] Init process (PID 1)
- [ ] Basic system call interface

### Phase 3: Device Drivers
- [ ] Keyboard driver (PS/2 + key repeat debouncing)
- [ ] Mouse driver (Virtio Tablet for absolute positioning)
- [ ] Framebuffer API and graphics driver
- [ ] Block device layer
- [ ] Virtio-Net driver

### Phase 4: File Systems
- [ ] EXT4 read support
- [ ] EXT4 write support
- [ ] Block bitmap and inode management
- [ ] Directory operations
- [ ] Indirect blocks

### Phase 5: Networking
- [ ] Ethernet frame handling
- [ ] ARP protocol
- [ ] IPv4 + ICMP
- [ ] UDP + TCP state machine
- [ ] DNS resolver
- [ ] Socket API

### Phase 6: GUI & Desktop Environment
- [ ] Double-buffered compositor
- [ ] Window manager (macOS-style)
- [ ] Widget toolkit
- [ ] Desktop shell (dock + panel)
- [ ] Theme system (dark/light + accent colors)
- [ ] Context menu API

### Phase 7: Applications
- [ ] Console (terminal emulator + shell)
- [ ] File Manager
- [ ] Text Editor (Notes)
- [ ] Image Viewer
- [ ] Media Player (MP3)
- [ ] Task Manager
- [ ] Calculator
- [ ] Clock
- [ ] System Settings
- [ ] System Installer

### Phase 8: Polish & Features
- [ ] Boot animation
- [ ] Live/Install boot selection
- [ ] Multiple keyboard layouts
- [ ] Python runtime
- [ ] Audio pipeline (Intel HDA)
- [ ] Dynamic wallpaper system
- [ ] JPEG/PNG decoders
- [ ] Neofetch-style system info tool

---

## Contributing

Contributions are welcome! Please follow these guidelines:

1. **Fork** the repository and create a feature branch
2. Follow the existing code style (kernel C with `snake_case`, tabs for indentation in Makefiles)
3. All kernel code must be **freestanding** — no standard library dependencies
4. Architecture-specific code goes in `kernel/arch/<arch>/`; use the HAL for shared interfaces
5. Test on QEMU before submitting a pull request
6. Include clear commit messages describing what and why

### Code Style

- C11 standard (`-std=gnu11`)
- `snake_case` for functions and variables
- `UPPER_CASE` for constants and macros
- All headers use include guards: `#ifndef _SECTION_NAME_H`
- Every source file begins with the license header comment

---

## Running in QEMU

### x86_64
```bash
qemu-system-x86_64 -M q35 -m 512M -serial stdio \
    -cdrom build/atomical-x86_64.iso
```

### ARM64
```bash
qemu-system-aarch64 -M virt -cpu cortex-a72 -m 512M -serial stdio \
    -cdrom build/atomical-arm64.iso
```

### With Networking
```bash
qemu-system-x86_64 -M q35 -m 512M -serial stdio \
    -netdev user,id=net0 -device virtio-net-pci,netdev=net0 \
    -cdrom build/atomical-x86_64.iso
```

---

## License

Atomical is released under the **MIT License**. See [LICENSE](LICENSE) for details.

Copyright (c) 2026 Dyno
