#include "kernel.h"

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

void gdt_init(void) {}
void idt_init(void) {}

void pic_init(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

void paging_init(void) {}
void pmm_init(void) {}
void acpi_init(void) {}
void ps2_keyboard_init(void) {}
void ps2_mouse_init(void) {}

void hlt_forever(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
