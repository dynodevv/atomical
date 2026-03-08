/*
 * Atomical OS - ARM64 Generic Timer
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <arch/arm64/arch.h>
#include <hal/hal.h>

static volatile uint64_t tick_count = 0;
static uint64_t timer_freq = 0;
static uint64_t timer_interval = 0;

/* Timer IRQ handler (PPI 30 on QEMU virt) */
static void arm_timer_irq_handler(uint32_t irq, void *context)
{
    (void)irq;
    (void)context;
    tick_count++;

    /* Reload timer */
    SYSREG_WRITE(CNTV_TVAL_EL0, timer_interval);
}

void hal_timer_init(uint32_t frequency_hz)
{
    /* Read the counter frequency */
    timer_freq = arm_timer_get_frequency();
    timer_interval = timer_freq / frequency_hz;

    kprintf("[timer] ARM Generic Timer: freq=%lu Hz, interval=%lu\n",
            timer_freq, timer_interval);

    /* Set the timer value */
    SYSREG_WRITE(CNTV_TVAL_EL0, timer_interval);

    /* Enable the virtual timer */
    SYSREG_WRITE(CNTV_CTL_EL0, 1);

    /* Register IRQ handler for virtual timer (IRQ 27 / PPI) */
    hal_irq_register(27, arm_timer_irq_handler, NULL);
}

uint64_t hal_timer_get_ticks(void)
{
    return tick_count;
}

uint64_t hal_timer_get_ns(void)
{
    if (timer_freq == 0) return 0;
    uint64_t count = arm_timer_get_count();
    return (count * 1000000000ULL) / timer_freq;
}

void hal_timer_udelay(uint64_t microseconds)
{
    uint64_t start = arm_timer_get_count();
    uint64_t target = (microseconds * timer_freq) / 1000000;
    while ((arm_timer_get_count() - start) < target)
        ;
}

/* ARM timer utility functions */

uint64_t arm_timer_get_count(void)
{
    return SYSREG_READ(CNTVCT_EL0);
}

uint64_t arm_timer_get_frequency(void)
{
    return SYSREG_READ(CNTFRQ_EL0);
}

void arm_timer_enable(void)
{
    SYSREG_WRITE(CNTV_CTL_EL0, 1);
}

void arm_timer_disable(void)
{
    SYSREG_WRITE(CNTV_CTL_EL0, 0);
}
