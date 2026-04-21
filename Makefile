# CachyOS deps: pacman -S --needed base-devel gcc nasm xorriso mtools qemu-desktop

ARCH := x86_64
BUILD_DIR := build
ISO_ROOT := $(BUILD_DIR)/iso_root
KERNEL := $(BUILD_DIR)/atomical.elf
ISO := atomical.iso
LIMINE_DIR := limine
LIMINE_BRANCH := v8.x-binary

CC := gcc
LD := ld
CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -m64 -O2 -Wall -Wextra -Ikernel/include
LDFLAGS := -nostdlib -z max-page-size=0x1000 -T /home/runner/work/atomical/atomical/kernel/linker.ld

KERNEL_C_SRCS := $(wildcard kernel/src/*.c)
KERNEL_S_SRCS := $(wildcard kernel/src/*.S)
KERNEL_OBJS := $(patsubst kernel/src/%.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SRCS)) \
               $(patsubst kernel/src/%.S,$(BUILD_DIR)/%.o,$(KERNEL_S_SRCS))

.PHONY: all clean run

all: $(ISO)

$(LIMINE_DIR):
	git clone --branch=$(LIMINE_BRANCH) --depth=1 https://github.com/limine-bootloader/limine.git $(LIMINE_DIR)

$(LIMINE_DIR)/limine: $(LIMINE_DIR)
	$(MAKE) -C $(LIMINE_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: kernel/src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: kernel/src/%.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

$(ISO): $(KERNEL) $(LIMINE_DIR) $(LIMINE_DIR)/limine
	mkdir -p $(ISO_ROOT)/boot $(ISO_ROOT)/boot/limine $(ISO_ROOT)/EFI/BOOT
	cp /home/runner/work/atomical/atomical/boot/limine.cfg $(ISO_ROOT)/boot/limine/limine.cfg
	cp $(KERNEL) $(ISO_ROOT)/boot/atomical.elf
	cp $(LIMINE_DIR)/limine-bios.sys $(ISO_ROOT)/boot/limine/
	cp $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_ROOT)/boot/limine/
	cp $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine/
	cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/
	xorriso -as mkisofs \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_ROOT) -o $(ISO)
	$(LIMINE_DIR)/limine bios-install $(ISO)

run: all
	qemu-system-x86_64 -enable-kvm -m 2G -cdrom $(ISO)

clean:
	rm -rf $(BUILD_DIR) $(ISO)
