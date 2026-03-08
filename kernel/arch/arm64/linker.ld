/*
 * Atomical OS - ARM64 Kernel Linker Script
 * For use with the Limine boot protocol.
 */

OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)
ENTRY(_start)

PHDRS
{
    limine_requests PT_LOAD;
    text    PT_LOAD FLAGS((1 << 0) | (1 << 2));      /* Read + Execute */
    rodata  PT_LOAD FLAGS((1 << 2));                  /* Read only */
    data    PT_LOAD FLAGS((1 << 1) | (1 << 2));       /* Read + Write */
}

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    . = ALIGN(4096);

    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(4096);

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(4096);

    .data : {
        *(.data .data.*)
    } :data

    . = ALIGN(4096);

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } :data

    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
        *(.comment)
    }
}
