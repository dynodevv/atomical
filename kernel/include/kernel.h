#ifndef ATOMICAL_KERNEL_H
#define ATOMICAL_KERNEL_H

#include <stddef.h>
#include <stdint.h>

void kernel_main(void);
void gdt_init(void);
void idt_init(void);
void pic_init(void);
void paging_init(void);
void pmm_init(void);
void acpi_init(void);
void ps2_keyboard_init(void);
void ps2_mouse_init(void);
void hlt_forever(void) __attribute__((noreturn));

#endif
