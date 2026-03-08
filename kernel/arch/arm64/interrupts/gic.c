/*
 * Atomical OS - ARM64 GICv3 Interrupt Controller
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <arch/arm64/arch.h>
#include <hal/hal.h>

/* GICv3 Distributor registers */
#define GICD_CTLR       0x0000
#define GICD_ISENABLER  0x0100
#define GICD_ICENABLER  0x0180
#define GICD_IPRIORITYR 0x0400
#define GICD_ITARGETSR  0x0800
#define GICD_ICFGR      0x0C00

/* GICv3 Redistributor registers */
#define GICR_WAKER      0x0014

/* System register interface (ICC) */
#define ICC_SRE_EL1     S3_0_C12_C12_5
#define ICC_PMR_EL1     S3_0_C4_C6_0
#define ICC_IAR1_EL1    S3_0_C12_C12_0
#define ICC_EOIR1_EL1   S3_0_C12_C12_1
#define ICC_IGRPEN1_EL1 S3_0_C12_C12_7

static volatile uint32_t *gicd = (volatile uint32_t *)GICD_BASE;

static irq_handler_t arm64_irq_handlers[1024];
static void         *arm64_irq_contexts[1024];

static inline void gicd_write(uint32_t offset, uint32_t val)
{
    *(volatile uint32_t *)((uint8_t *)gicd + offset) = val;
}

static inline uint32_t gicd_read(uint32_t offset)
{
    return *(volatile uint32_t *)((uint8_t *)gicd + offset);
}

void gicv3_init(void)
{
    /* Enable system register interface */
    SYSREG_WRITE(ICC_SRE_EL1, SYSREG_READ(ICC_SRE_EL1) | 0x1);
    arm64_isb();

    /* Set priority mask to accept all priorities */
    SYSREG_WRITE(ICC_PMR_EL1, 0xFF);

    /* Enable Group 1 interrupts */
    SYSREG_WRITE(ICC_IGRPEN1_EL1, 0x1);

    /* Enable distributor */
    gicd_write(GICD_CTLR, 0x7);  /* Enable all groups */

    arm64_isb();
    kprintf("[gic]  GICv3 initialized\n");
}

void gicv3_enable_irq(uint32_t irq)
{
    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;
    gicd_write(GICD_ISENABLER + reg * 4, BIT(bit));
}

void gicv3_disable_irq(uint32_t irq)
{
    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;
    gicd_write(GICD_ICENABLER + reg * 4, BIT(bit));
}

uint32_t gicv3_ack_irq(void)
{
    uint32_t iar;
    __asm__ volatile("mrs %0, " "S3_0_C12_C12_0" : "=r"(iar));
    return iar & 0x3FF;
}

void gicv3_send_eoi(uint32_t irq)
{
    __asm__ volatile("msr " "S3_0_C12_C12_1" ", %0" : : "r"((uint64_t)irq));
}

/* Exception handlers called from assembly */

void arm64_handle_sync(void *frame)
{
    (void)frame;
    uint64_t esr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));

    uint64_t far;
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));

    uint32_t ec = (esr >> 26) & 0x3F;

    kprintf("[exception] Synchronous: ESR=0x%lx, EC=0x%x, FAR=0x%lx\n", esr, ec, far);

    if (ec == 0x24 || ec == 0x25) {
        /* Data/Instruction abort */
        panic("Memory abort at 0x%lx", far);
    } else if (ec == 0x15) {
        /* SVC (system call) from AArch64 */
        kprintf("[syscall] SVC from userspace\n");
    }
}

void arm64_handle_irq(void *frame)
{
    (void)frame;
    uint32_t irq = gicv3_ack_irq();

    if (irq < 1024 && arm64_irq_handlers[irq]) {
        arm64_irq_handlers[irq](irq, arm64_irq_contexts[irq]);
    }

    gicv3_send_eoi(irq);
}

/* HAL interrupt functions for ARM64 */

void hal_interrupts_init(void)
{
    /* Install exception vector table */
    extern void arm64_exception_vectors(void);
    SYSREG_WRITE(VBAR_EL1, (uint64_t)arm64_exception_vectors);
    arm64_isb();

    gicv3_init();
}

void hal_interrupts_enable(void)
{
    arm64_enable_interrupts();
}

void hal_interrupts_disable(void)
{
    arm64_disable_interrupts();
}

bool hal_interrupts_enabled(void)
{
    return (arm64_read_daif() & 0x3C0) == 0;
}

int hal_irq_register(uint32_t irq, irq_handler_t handler, void *context)
{
    if (irq >= 1024) return -1;
    arm64_irq_handlers[irq] = handler;
    arm64_irq_contexts[irq] = context;
    gicv3_enable_irq(irq);
    return 0;
}

void hal_irq_unregister(uint32_t irq)
{
    if (irq < 1024) {
        gicv3_disable_irq(irq);
        arm64_irq_handlers[irq] = NULL;
        arm64_irq_contexts[irq] = NULL;
    }
}

void hal_irq_ack(uint32_t irq)
{
    gicv3_send_eoi(irq);
}

void hal_irq_unmask(uint32_t irq)
{
    gicv3_enable_irq(irq);
}

void hal_irq_mask(uint32_t irq)
{
    gicv3_disable_irq(irq);
}
