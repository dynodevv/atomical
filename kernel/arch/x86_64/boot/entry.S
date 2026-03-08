/*
 * Atomical OS - x86_64 Boot Entry (Assembly)
 * Limine protocol entry point for AMD64.
 * MIT License - Copyright (c) 2026 Dyno
 */

.section .text
.global _start
.extern kmain

_start:
    /* Clear direction flag */
    cld

    /* Zero out BSS - Limine should do this, but be safe */
    xor %rdi, %rdi

    /* Set up a known-good stack (Limine provides one, but we ensure alignment) */
    and $~0xF, %rsp

    /* Call the C kernel entry point */
    call kmain

    /* If kmain returns (it shouldn't), halt */
.hang:
    cli
    hlt
    jmp .hang

/*
 * Interrupt stub macros - save/restore full context
 */
.macro ISR_NOERR num
.global isr_stub_\num
isr_stub_\num:
    push $0          /* dummy error code */
    push $\num       /* interrupt number */
    jmp isr_common
.endm

.macro ISR_ERR num
.global isr_stub_\num
isr_stub_\num:
    push $\num       /* interrupt number (error code already pushed by CPU) */
    jmp isr_common
.endm

.extern x86_64_exception_handler

isr_common:
    /* Save general purpose registers */
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    /* Pass the stack pointer as argument (pointer to saved registers) */
    mov %rsp, %rdi
    call x86_64_exception_handler

    /* Restore registers */
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    /* Remove interrupt number and error code */
    add $16, %rsp

    iretq

/* CPU exception stubs (0-31) */
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

/* IRQ stubs (32-47) */
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47

/*
 * Context switch: hal_context_switch(old_ctx, new_ctx)
 * %rdi = old_ctx, %rsi = new_ctx
 */
.global hal_context_switch
hal_context_switch:
    /* Save callee-saved registers to old context */
    mov %r15,  0(%rdi)
    mov %r14,  8(%rdi)
    mov %r13, 16(%rdi)
    mov %r12, 24(%rdi)
    mov %rbp, 32(%rdi)
    mov %rbx, 40(%rdi)
    mov %rsp, 48(%rdi)
    /* Save return address (rip) */
    lea 1f(%rip), %rax
    mov %rax, 56(%rdi)

    /* Restore callee-saved registers from new context */
    mov  0(%rsi), %r15
    mov  8(%rsi), %r14
    mov 16(%rsi), %r13
    mov 24(%rsi), %r12
    mov 32(%rsi), %rbp
    mov 40(%rsi), %rbx
    mov 48(%rsi), %rsp
    /* Jump to new context's saved rip */
    jmp *56(%rsi)
1:
    ret

/*
 * Enter userspace: hal_userspace_enter(entry, user_stack, arg)
 * Uses iretq to transition to Ring 3.
 */
.global hal_userspace_enter
hal_userspace_enter:
    /* %rdi = entry point, %rsi = user stack, %rdx = arg */
    mov $0x23, %rax    /* user data segment (GDT_USER_DATA | RPL3) */
    push %rax          /* SS */
    push %rsi          /* RSP (user stack) */
    pushf
    pop %rax
    or $0x200, %rax    /* Enable interrupts in user mode */
    push %rax          /* RFLAGS */
    mov $0x1B, %rax    /* user code segment (GDT_USER_CODE | RPL3) */
    push %rax          /* CS */
    push %rdi          /* RIP (entry point) */
    mov %rdx, %rdi     /* arg -> first parameter */
    xor %rax, %rax     /* Clear registers for security */
    xor %rbx, %rbx
    xor %rcx, %rcx
    xor %rdx, %rdx
    xor %rsi, %rsi
    xor %rbp, %rbp
    xor %r8, %r8
    xor %r9, %r9
    xor %r10, %r10
    xor %r11, %r11
    xor %r12, %r12
    xor %r13, %r13
    xor %r14, %r14
    xor %r15, %r15
    iretq
