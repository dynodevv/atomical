/*
 * Atomical OS - x86_64 Timer (PIT + APIC Timer scaffolding)
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <arch/x86_64/arch.h>
#include <hal/hal.h>

/* PIT (Programmable Interval Timer) ports */
#define PIT_CHANNEL0  0x40
#define PIT_COMMAND   0x43
#define PIT_FREQUENCY 1193182

static volatile uint64_t tick_count = 0;

/* Timer IRQ handler (IRQ 0) */
static void timer_irq_handler(uint32_t irq, void *context)
{
    (void)irq;
    (void)context;
    tick_count++;
}

void hal_timer_init(uint32_t frequency_hz)
{
    uint16_t divisor = (uint16_t)(PIT_FREQUENCY / frequency_hz);

    /* Channel 0, access mode lo/hi, mode 2 (rate generator) */
    outb(PIT_COMMAND, 0x34);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    hal_irq_register(0, timer_irq_handler, NULL);

    kprintf("[timer] PIT initialized at %u Hz (divisor=%u)\n", frequency_hz, divisor);
}

uint64_t hal_timer_get_ticks(void)
{
    return tick_count;
}

uint64_t hal_timer_get_ns(void)
{
    /* Approximate: assuming 1000 Hz tick rate */
    return tick_count * 1000000;
}

void hal_timer_udelay(uint64_t microseconds)
{
    uint64_t target = tick_count + (microseconds / 1000) + 1;
    while (tick_count < target)
        hal_cpu_idle();
}
