/*
 * Atomical OS - ARM64 Boot Entry (Assembly)
 * Limine protocol entry point for AArch64.
 * MIT License - Copyright (c) 2026 Dyno
 */

.section .text
.global _start
.extern kmain

_start:
    /* Ensure we're running at EL1 (Limine drops us here) */
    mrs x0, CurrentEL
    lsr x0, x0, #2
    cmp x0, #1
    b.ne .hang

    /* Stack is provided by Limine, ensure alignment */
    mov x0, sp
    and x0, x0, #~0xF
    mov sp, x0

    /* Clear BSS */
    adrp x0, __bss_start
    add  x0, x0, :lo12:__bss_start
    adrp x1, __bss_end
    add  x1, x1, :lo12:__bss_end
.bss_clear:
    cmp x0, x1
    b.ge .bss_done
    str xzr, [x0], #8
    b .bss_clear
.bss_done:

    /* Call kernel main */
    bl kmain

.hang:
    wfi
    b .hang

/*
 * Exception vector table for ARM64 (EL1)
 * Each entry is 128 bytes (32 instructions), 16 entries total.
 */
.balign 2048
.global arm64_exception_vectors
arm64_exception_vectors:

/* --- Current EL with SP_EL0 --- */
/* Synchronous */
.balign 128
    b arm64_sync_handler
/* IRQ */
.balign 128
    b arm64_irq_handler
/* FIQ */
.balign 128
    b arm64_fiq_handler
/* SError */
.balign 128
    b arm64_serror_handler

/* --- Current EL with SP_ELx --- */
/* Synchronous */
.balign 128
    b arm64_sync_handler
/* IRQ */
.balign 128
    b arm64_irq_handler
/* FIQ */
.balign 128
    b arm64_fiq_handler
/* SError */
.balign 128
    b arm64_serror_handler

/* --- Lower EL using AArch64 --- */
/* Synchronous */
.balign 128
    b arm64_lower_sync_handler
/* IRQ */
.balign 128
    b arm64_lower_irq_handler
/* FIQ */
.balign 128
    b arm64_fiq_handler
/* SError */
.balign 128
    b arm64_serror_handler

/* --- Lower EL using AArch32 --- */
/* Synchronous */
.balign 128
    b arm64_sync_handler
/* IRQ */
.balign 128
    b arm64_irq_handler
/* FIQ */
.balign 128
    b arm64_fiq_handler
/* SError */
.balign 128
    b arm64_serror_handler

/*
 * Save all general-purpose registers (exception frame)
 */
.macro SAVE_REGS
    sub sp, sp, #256
    stp x0,  x1,  [sp, #0]
    stp x2,  x3,  [sp, #16]
    stp x4,  x5,  [sp, #32]
    stp x6,  x7,  [sp, #48]
    stp x8,  x9,  [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x30, x0,  [sp, #240]
    /* x1 (spsr) not saved in frame directly for simplicity */
.endm

.macro RESTORE_REGS
    ldp x30, x0,  [sp, #240]
    msr elr_el1, x0
    ldp x0,  x1,  [sp, #0]
    ldp x2,  x3,  [sp, #16]
    ldp x4,  x5,  [sp, #32]
    ldp x6,  x7,  [sp, #48]
    ldp x8,  x9,  [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    add sp, sp, #256
.endm

.extern arm64_handle_sync
.extern arm64_handle_irq

arm64_sync_handler:
    SAVE_REGS
    mov x0, sp
    bl arm64_handle_sync
    RESTORE_REGS
    eret

arm64_irq_handler:
    SAVE_REGS
    mov x0, sp
    bl arm64_handle_irq
    RESTORE_REGS
    eret

arm64_fiq_handler:
    b arm64_fiq_handler    /* FIQ not used */

arm64_serror_handler:
    b arm64_serror_handler /* SError = panic */

arm64_lower_sync_handler:
    SAVE_REGS
    mov x0, sp
    bl arm64_handle_sync
    RESTORE_REGS
    eret

arm64_lower_irq_handler:
    SAVE_REGS
    mov x0, sp
    bl arm64_handle_irq
    RESTORE_REGS
    eret

/*
 * hal_userspace_enter(entry, user_stack, arg)
 * x0 = entry, x1 = user_stack, x2 = arg
 * Uses eret to drop to EL0.
 */
.global hal_userspace_enter
hal_userspace_enter:
    msr elr_el1, x0          /* Return address = entry point */
    mov x0, #0               /* SPSR: EL0t, interrupts enabled */
    msr spsr_el1, x0
    msr sp_el0, x1           /* User stack */
    mov x0, x2               /* First argument */
    mov x1, xzr              /* Clear registers for security */
    mov x2, xzr
    mov x3, xzr
    eret

/*
 * hal_context_switch(old_ctx, new_ctx)
 * x0 = old_ctx, x1 = new_ctx
 */
.global hal_context_switch
hal_context_switch:
    /* Save callee-saved registers */
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]
    mov x2, sp
    str x2, [x0, #96]

    /* Restore callee-saved registers */
    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]
    ldr x2, [x1, #96]
    mov sp, x2

    ret
