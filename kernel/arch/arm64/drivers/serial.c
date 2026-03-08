/*
 * Atomical OS - ARM64 PL011 UART Driver
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <arch/arm64/arch.h>
#include <hal/hal.h>

/* PL011 Register offsets */
#define PL011_DR      0x000  /* Data Register */
#define PL011_FR      0x018  /* Flag Register */
#define PL011_IBRD    0x024  /* Integer Baud Rate Divisor */
#define PL011_FBRD    0x028  /* Fractional Baud Rate Divisor */
#define PL011_LCRH    0x02C  /* Line Control Register */
#define PL011_CR      0x030  /* Control Register */
#define PL011_IMSC    0x038  /* Interrupt Mask Set/Clear */
#define PL011_ICR     0x044  /* Interrupt Clear Register */

/* Flag Register bits */
#define PL011_FR_TXFF BIT(5)  /* Transmit FIFO Full */
#define PL011_FR_RXFE BIT(4)  /* Receive FIFO Empty */

static volatile uint8_t *uart_base = (volatile uint8_t *)PL011_BASE;

static inline void uart_write(uint32_t offset, uint32_t val)
{
    *(volatile uint32_t *)(uart_base + offset) = val;
}

static inline uint32_t uart_read(uint32_t offset)
{
    return *(volatile uint32_t *)(uart_base + offset);
}

void pl011_init(void)
{
    /* Disable UART */
    uart_write(PL011_CR, 0);

    /* Clear all interrupts */
    uart_write(PL011_ICR, 0x7FF);

    /* Set baud rate: 115200 with 48 MHz clock
     * IBRD = 48000000 / (16 * 115200) = 26
     * FBRD = round(0.041666 * 64) = 3 */
    uart_write(PL011_IBRD, 26);
    uart_write(PL011_FBRD, 3);

    /* 8 bits, no parity, 1 stop bit, enable FIFO */
    uart_write(PL011_LCRH, (3 << 5) | BIT(4));

    /* Enable UART, TX, RX */
    uart_write(PL011_CR, BIT(0) | BIT(8) | BIT(9));
}

void pl011_putc(char c)
{
    /* Wait until TX FIFO is not full */
    while (uart_read(PL011_FR) & PL011_FR_TXFF)
        ;
    uart_write(PL011_DR, (uint32_t)c);
}

char pl011_getc(void)
{
    /* Wait until RX FIFO is not empty */
    while (uart_read(PL011_FR) & PL011_FR_RXFE)
        ;
    return (char)(uart_read(PL011_DR) & 0xFF);
}

/* HAL serial implementation for ARM64 */

void hal_serial_init(void)
{
    pl011_init();
}

void hal_serial_putc(char c)
{
    if (c == '\n')
        pl011_putc('\r');
    pl011_putc(c);
}

void hal_serial_puts(const char *s)
{
    while (*s)
        hal_serial_putc(*s++);
}
