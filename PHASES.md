# Atomical OS — Development Phases

This document outlines the development roadmap for Atomical OS, broken down into
eight sequential phases. Each phase builds on the previous one.

---

## Phase 1: Core Kernel Foundation ✅

The bare-metal kernel boots on x86_64 and ARM64, initializes hardware, and provides
the fundamental subsystems that everything else is built on.

- [x] Project structure and build system (multi-arch Makefile, linker scripts)
- [x] Limine boot protocol integration (v8.x, UEFI + BIOS)
- [x] x86_64 and ARM64 boot entry points (assembly `_start`, BSS clearing)
- [x] Serial console output (x86_64 16550 UART / ARM64 PL011)
- [x] Physical memory manager (bitmap-based frame allocator)
- [x] Kernel heap allocator (linked-list with split/coalesce)
- [x] 4-level paging with NX bit (x86_64 PML4 / ARM64 PGD)
- [x] Interrupt handling (x86_64 IDT + PIC / ARM64 GICv3 + exception vectors)
- [x] System timer (x86_64 PIT / ARM64 Generic Timer)
- [x] VFS core + RamFS (inodes, dentries, mount table, path resolution)
- [x] Hardware Abstraction Layer (uniform C API across architectures)
- [x] CI/CD pipeline (GitHub Actions — build x86_64, ARM64, validate sources)

---

## Phase 2: Process Management & Userspace

Preemptive multitasking, ELF loading, and the system call interface needed to run
userspace programs.

- [x] Scheduler implementation (round-robin with cooperative yielding)
- [ ] ELF binary loader
- [ ] `sys_execve` system call
- [ ] User stack initialization
- [ ] Init process (PID 1)
- [ ] Basic system call interface (syscall/sysret on x86_64, SVC on ARM64)

---

## Phase 3: Device Drivers

Input, display, block storage, and network device drivers.

- [x] Keyboard driver (PS/2 + key repeat debouncing)
- [ ] Mouse driver (Virtio Tablet for absolute positioning)
- [x] Framebuffer API and graphics driver
- [ ] Block device layer
- [ ] Virtio-Net driver

---

## Phase 4: File Systems

Persistent storage via EXT4 on top of the block device layer.

- [ ] EXT4 read support
- [ ] EXT4 write support
- [ ] Block bitmap and inode management
- [ ] Directory operations
- [ ] Indirect blocks

---

## Phase 5: Networking

A full TCP/IP stack from Ethernet frames up to DNS resolution.

- [ ] Ethernet frame handling
- [ ] ARP protocol
- [ ] IPv4 + ICMP
- [ ] UDP + TCP state machine
- [ ] DNS resolver
- [ ] Socket API

---

## Phase 6: GUI & Desktop Environment

A composited graphical desktop with a macOS-inspired design language.

- [ ] Double-buffered compositor (alpha blending, z-ordering)
- [ ] Window manager (macOS-style title bar, close/minimize/maximize)
- [ ] Widget toolkit (buttons, labels, text fields, scroll views)
- [ ] Desktop shell (dock + top panel)
- [ ] Theme system (dark/light mode + accent colors)
- [ ] Context menu API

---

## Phase 7: Applications

Built-in applications that ship with the OS.

- [ ] Console
  - A terminal emulator + shell
- [ ] Files
  - A file manager
- [ ] Notes
  - A text editor
- [ ] Viewer
  - A JPEG/PNG image viewer
- [ ] Player
  - A MP3 audio player
- [ ] Processes
  - A task manager
- [ ] Calculate
  - A calculator
- [ ] Time
  - A clock, stopwatch and timer app
- [ ] Configure
  - A system settings app
- [ ] Install
  - A system installer app

---

## Phase 8: Polish & Features

Final integration, multimedia support, and user-facing polish.

- [ ] Boot animation (replacing static splash)
- [ ] Live / Install boot selection
- [ ] Multiple keyboard layouts (US, Hungarian, European)
- [ ] Python runtime
- [ ] Audio pipeline (Intel HDA driver)
- [ ] Dynamic wallpaper system
- [ ] JPEG / PNG decoders
- [ ] Neofetch-style system info tool
