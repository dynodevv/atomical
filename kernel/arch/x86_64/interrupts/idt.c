/*
 * Atomical OS - x86_64 IDT and Interrupt Handling
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <arch/x86_64/arch.h>
#include <hal/hal.h>

/* IDT and IDTR */
static idt_entry_t idt[IDT_ENTRIES] ALIGNED(16);
static idt_ptr_t   idtr;

/* IRQ handler table */
static irq_handler_t irq_handlers[IDT_ENTRIES];
static void         *irq_contexts[IDT_ENTRIES];

/* External ISR stubs from entry.S */
extern void isr_stub_0(void);
extern void isr_stub_1(void);
extern void isr_stub_2(void);
extern void isr_stub_3(void);
extern void isr_stub_4(void);
extern void isr_stub_5(void);
extern void isr_stub_6(void);
extern void isr_stub_7(void);
extern void isr_stub_8(void);
extern void isr_stub_9(void);
extern void isr_stub_10(void);
extern void isr_stub_11(void);
extern void isr_stub_12(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);
extern void isr_stub_15(void);
extern void isr_stub_16(void);
extern void isr_stub_17(void);
extern void isr_stub_18(void);
extern void isr_stub_19(void);
extern void isr_stub_20(void);
extern void isr_stub_21(void);
extern void isr_stub_22(void);
extern void isr_stub_23(void);
extern void isr_stub_24(void);
extern void isr_stub_25(void);
extern void isr_stub_26(void);
extern void isr_stub_27(void);
extern void isr_stub_28(void);
extern void isr_stub_29(void);
extern void isr_stub_30(void);
extern void isr_stub_31(void);
extern void isr_stub_32(void);
extern void isr_stub_33(void);
extern void isr_stub_34(void);
extern void isr_stub_35(void);
extern void isr_stub_36(void);
extern void isr_stub_37(void);
extern void isr_stub_38(void);
extern void isr_stub_39(void);
extern void isr_stub_40(void);
extern void isr_stub_41(void);
extern void isr_stub_42(void);
extern void isr_stub_43(void);
extern void isr_stub_44(void);
extern void isr_stub_45(void);
extern void isr_stub_46(void);
extern void isr_stub_47(void);

/* Exception names */
static const char *exception_names[] = {
    "Division Error", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 FP Exception", "Alignment Check", "Machine Check", "SIMD FP Exception",
    "Virtualization Exception", "Control Protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved",
};

void idt_set_gate(uint8_t vector, uintptr_t handler, uint8_t ist, uint8_t type_attr)
{
    idt[vector].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector    = GDT_KERNEL_CODE;
    idt[vector].ist         = ist & 0x07;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].reserved    = 0;
}

void idt_init(void)
{
    memset(idt, 0, sizeof(idt));
    memset(irq_handlers, 0, sizeof(irq_handlers));

    /* Set up exception handlers (0-31) */
    typedef void (*isr_stub_fn)(void);
    isr_stub_fn stubs[48] = {
        isr_stub_0,  isr_stub_1,  isr_stub_2,  isr_stub_3,
        isr_stub_4,  isr_stub_5,  isr_stub_6,  isr_stub_7,
        isr_stub_8,  isr_stub_9,  isr_stub_10, isr_stub_11,
        isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15,
        isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19,
        isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23,
        isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27,
        isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31,
        isr_stub_32, isr_stub_33, isr_stub_34, isr_stub_35,
        isr_stub_36, isr_stub_37, isr_stub_38, isr_stub_39,
        isr_stub_40, isr_stub_41, isr_stub_42, isr_stub_43,
        isr_stub_44, isr_stub_45, isr_stub_46, isr_stub_47,
    };

    /* Interrupt gate: 0x8E = present, ring 0, interrupt gate */
    for (int i = 0; i < 48; i++) {
        idt_set_gate(i, (uintptr_t)stubs[i], 0, 0x8E);
    }

    /* Load IDT */
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;
    __asm__ volatile("lidt %0" : : "m"(idtr));

    kprintf("[idt]  IDT loaded: %d entries\n", IDT_ENTRIES);
}

/* Exception handler called from assembly stub */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} interrupt_frame_t;

void x86_64_exception_handler(interrupt_frame_t *frame)
{
    uint64_t int_no = frame->int_no;

    if (int_no < 32) {
        /* CPU exception */
        if (int_no == 14) {
            /* Page fault - get faulting address from CR2 */
            uint64_t cr2 = read_cr2();
            kprintf("[fault] Page Fault at 0x%lx, error=0x%lx, RIP=0x%lx\n",
                    cr2, frame->error_code, frame->rip);
        } else {
            kprintf("[fault] Exception %lu: %s (error=0x%lx, RIP=0x%lx)\n",
                    int_no, exception_names[int_no], frame->error_code, frame->rip);
        }

        /* Fatal exceptions cause a panic */
        if (int_no == 8 || int_no == 13 || int_no == 14) {
            panic("Unrecoverable CPU exception: %s", exception_names[int_no]);
        }
    } else if (int_no < 48) {
        /* Hardware IRQ */
        uint32_t irq = (uint32_t)(int_no - 32);
        if (irq_handlers[int_no]) {
            irq_handlers[int_no](irq, irq_contexts[int_no]);
        }
        hal_irq_ack(irq);
    }
}

/* PIC remapping: move IRQs 0-15 to vectors 32-47 */
static void pic_remap(void)
{
    /* ICW1: begin initialization in cascade mode */
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();

    /* ICW2: vector offsets — master IRQs at 32, slave at 40 */
    outb(0x21, 0x20);  /* master offset = 32 */
    io_wait();
    outb(0xA1, 0x28);  /* slave offset = 40 */
    io_wait();

    /* ICW3: cascade wiring */
    outb(0x21, 0x04);  /* master: slave on IRQ2 */
    io_wait();
    outb(0xA1, 0x02);  /* slave: cascade identity */
    io_wait();

    /* ICW4: 8086 mode */
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    /* Mask all IRQs; drivers unmask individually when they register */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

/* HAL interrupt functions */

void hal_interrupts_init(void)
{
    pic_remap();
    idt_init();
    /* TODO: Initialize APIC/IOAPIC */
}

void hal_interrupts_enable(void)
{
    sti();
}

void hal_interrupts_disable(void)
{
    cli();
}

bool hal_interrupts_enabled(void)
{
    return (read_rflags() & 0x200) != 0;
}

int hal_irq_register(uint32_t irq, irq_handler_t handler, void *context)
{
    uint32_t vector = irq + 32;
    if (vector >= IDT_ENTRIES)
        return -1;
    irq_handlers[vector] = handler;
    irq_contexts[vector] = context;
    return 0;
}

void hal_irq_unregister(uint32_t irq)
{
    uint32_t vector = irq + 32;
    if (vector < IDT_ENTRIES) {
        irq_handlers[vector] = NULL;
        irq_contexts[vector] = NULL;
    }
}

void hal_irq_ack(uint32_t irq)
{
    /* PIC EOI (will be replaced by APIC) */
    if (irq >= 8)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void hal_irq_unmask(uint32_t irq)
{
    uint16_t port = (irq < 8) ? 0x21 : 0xA1;
    uint8_t  bit  = (uint8_t)(irq & 7);
    outb(port, inb(port) & ~(1 << bit));
}

void hal_irq_mask(uint32_t irq)
{
    uint16_t port = (irq < 8) ? 0x21 : 0xA1;
    uint8_t  bit  = (uint8_t)(irq & 7);
    outb(port, inb(port) | (1 << bit));
}
