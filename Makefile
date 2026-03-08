#
# Atomical OS - Top-Level Makefile
# Multi-architecture build system for the Atomical kernel.
# MIT License - Copyright (c) 2026 Dyno
#

# === Configuration ===
ARCH       ?= x86_64
BUILD_DIR  := build/$(ARCH)
ISO_DIR    := build/iso
KERNEL_ELF := $(BUILD_DIR)/atomical.elf
ISO_FILE   := build/atomical-$(ARCH).iso

# === Toolchain ===
ifeq ($(ARCH),x86_64)
    CC      := x86_64-elf-gcc
    AS      := x86_64-elf-as
    LD      := x86_64-elf-ld
    OBJCOPY := x86_64-elf-objcopy
    QEMU    := qemu-system-x86_64
    QEMU_FLAGS := -M q35 -m 512M -serial stdio -no-reboot -no-shutdown
    LDSCRIPT := kernel/arch/x86_64/linker.ld
else ifeq ($(ARCH),arm64)
    CC      := aarch64-elf-gcc
    AS      := aarch64-elf-as
    LD      := aarch64-elf-ld
    OBJCOPY := aarch64-elf-objcopy
    QEMU    := qemu-system-aarch64
    QEMU_FLAGS := -M virt -cpu cortex-a72 -m 512M -serial stdio -no-reboot -no-shutdown
    LDSCRIPT := kernel/arch/arm64/linker.ld
else ifeq ($(ARCH),i386)
    CC      := i686-elf-gcc
    AS      := i686-elf-as
    LD      := i686-elf-ld
    OBJCOPY := i686-elf-objcopy
    QEMU    := qemu-system-i386
    QEMU_FLAGS := -m 256M -serial stdio -no-reboot -no-shutdown
    LDSCRIPT := kernel/arch/x86_64/linker.ld
endif

# === Compiler Flags ===
INCLUDES := -Ikernel/include -Ikernel/include/arch/$(ARCH)

CFLAGS := -std=gnu11 -O2 -g \
          -Wall -Wextra -Werror \
          -ffreestanding -fno-stack-protector \
          -fno-pic \
          -nostdlib -nostdinc \
          $(INCLUDES)

ifeq ($(ARCH),x86_64)
    CFLAGS += -mcmodel=kernel -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -mno-80387
else ifeq ($(ARCH),arm64)
    CFLAGS += -mgeneral-regs-only
endif

ASFLAGS :=

LDFLAGS := -nostdlib -static -T $(LDSCRIPT)

# === Source Files ===
# Common kernel sources (architecture-independent)
COMMON_C_SRCS := \
    kernel/main.c \
    kernel/lib/klib.c \
    kernel/mm/pmm.c \
    kernel/mm/heap.c \
    kernel/sched/sched.c \
    kernel/test/ktest.c \
    fs/vfs/vfs.c \
    fs/ramfs/ramfs.c \
    drivers/display/fbconsole.c \
    drivers/input/keyboard.c

# Architecture-specific sources
ifeq ($(ARCH),x86_64)
    ARCH_ASM_SRCS := kernel/arch/x86_64/boot/entry.S
    ARCH_C_SRCS := \
        kernel/arch/x86_64/drivers/serial.c \
        kernel/arch/x86_64/interrupts/idt.c \
        kernel/arch/x86_64/drivers/timer.c \
        kernel/arch/x86_64/mm/paging.c
else ifeq ($(ARCH),arm64)
    ARCH_ASM_SRCS := kernel/arch/arm64/boot/entry.S
    ARCH_C_SRCS := \
        kernel/arch/arm64/drivers/serial.c \
        kernel/arch/arm64/interrupts/gic.c \
        kernel/arch/arm64/drivers/timer.c \
        kernel/arch/arm64/mm/paging.c
endif

# Object files
COMMON_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(COMMON_C_SRCS))
ARCH_C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(ARCH_C_SRCS))
ARCH_ASM_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(ARCH_ASM_SRCS))

ALL_OBJS := $(ARCH_ASM_OBJS) $(COMMON_OBJS) $(ARCH_C_OBJS)

# === Targets ===

.PHONY: all clean kernel iso run

all: kernel

kernel: $(KERNEL_ELF)

$(KERNEL_ELF): $(ALL_OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "[LD] $@"

# Compile C sources
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
	@echo "[CC] $<"

# Assemble ASM sources
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
	@echo "[AS] $<"

# === ISO Build ===
# Requires: xorriso, limine

iso: $(KERNEL_ELF)
	@echo "=== Building ISO for $(ARCH) ==="
	@mkdir -p $(ISO_DIR)/boot/limine
	@cp $(KERNEL_ELF) $(ISO_DIR)/boot/atomical.elf
	@cp iso/boot/limine/limine.conf $(ISO_DIR)/boot/limine/limine.conf
ifeq ($(ARCH),x86_64)
	@cp limine/limine-bios.sys $(ISO_DIR)/boot/limine/ 2>/dev/null || true
	@cp limine/limine-bios-cd.bin $(ISO_DIR)/boot/limine/ 2>/dev/null || true
	@cp limine/limine-uefi-cd.bin $(ISO_DIR)/boot/limine/ 2>/dev/null || true
	@mkdir -p $(ISO_DIR)/EFI/BOOT
	@cp limine/BOOTX64.EFI $(ISO_DIR)/EFI/BOOT/ 2>/dev/null || true
	xorriso -as mkisofs \
	    -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot -boot-load-size 4 -boot-info-table \
	    --efi-boot boot/limine/limine-uefi-cd.bin \
	    -efi-boot-part --efi-boot-image --protective-msdos-label \
	    $(ISO_DIR) -o $(ISO_FILE)
	limine bios-install $(ISO_FILE) 2>/dev/null || true
else ifeq ($(ARCH),arm64)
	@cp limine/limine-uefi-cd.bin $(ISO_DIR)/boot/limine/ 2>/dev/null || true
	@mkdir -p $(ISO_DIR)/EFI/BOOT
	@cp limine/BOOTAA64.EFI $(ISO_DIR)/EFI/BOOT/ 2>/dev/null || true
	xorriso -as mkisofs \
	    --efi-boot boot/limine/limine-uefi-cd.bin \
	    -efi-boot-part --efi-boot-image --protective-msdos-label \
	    $(ISO_DIR) -o $(ISO_FILE)
endif
	@echo "=== ISO built: $(ISO_FILE) ==="

# === Run in QEMU ===
run: iso
	$(QEMU) $(QEMU_FLAGS) -cdrom $(ISO_FILE)

run-kernel: $(KERNEL_ELF)
	$(QEMU) $(QEMU_FLAGS) -kernel $(KERNEL_ELF)

# === Clean ===
clean:
	rm -rf build/
