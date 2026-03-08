/*
 * Atomical OS - x86_64 Serial Driver (16550 UART / COM1)
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <arch/x86_64/arch.h>

/* COM1 port addresses */
#define COM1_PORT       0x3F8
#define COM1_DATA       (COM1_PORT + 0)
#define COM1_INT_EN     (COM1_PORT + 1)
#define COM1_FIFO_CTRL  (COM1_PORT + 2)
#define COM1_LINE_CTRL  (COM1_PORT + 3)
#define COM1_MODEM_CTRL (COM1_PORT + 4)
#define COM1_LINE_STAT  (COM1_PORT + 5)

/* Divisor latch (when DLAB is set) */
#define COM1_DIV_LO     (COM1_PORT + 0)
#define COM1_DIV_HI     (COM1_PORT + 1)

void hal_serial_init(void)
{
    /* Disable interrupts */
    outb(COM1_INT_EN, 0x00);

    /* Enable DLAB to set baud rate divisor */
    outb(COM1_LINE_CTRL, 0x80);

    /* Set divisor to 1 (115200 baud) */
    outb(COM1_DIV_LO, 0x01);
    outb(COM1_DIV_HI, 0x00);

    /* 8 bits, no parity, one stop bit (8N1) */
    outb(COM1_LINE_CTRL, 0x03);

    /* Enable FIFO, clear buffers, 14-byte threshold */
    outb(COM1_FIFO_CTRL, 0xC7);

    /* IRQs enabled, RTS/DSR set */
    outb(COM1_MODEM_CTRL, 0x0B);
}

static bool serial_is_transmit_empty(void)
{
    return (inb(COM1_LINE_STAT) & 0x20) != 0;
}

void hal_serial_putc(char c)
{
    while (!serial_is_transmit_empty())
        ;
    outb(COM1_DATA, (uint8_t)c);
}

void hal_serial_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            hal_serial_putc('\r');
        hal_serial_putc(*s++);
    }
}
