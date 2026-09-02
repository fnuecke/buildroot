/* SPDX-License-Identifier: MIT */
/*
 * The firmware region: text, data, bss and the trap/boot stack together must fit the size the
 * board reserves for resident firmware. The binary itself is smaller (bss is not emitted); the
 * region is the runtime footprint.
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY {
    FW (rwx) : ORIGIN = 0x80000000, LENGTH = 8K
}

STACK_SIZE = 2K;

SECTIONS {
    .text : {
        KEEP(*(.text.start))
        *(.text .text.*)
    } > FW

    .rodata : {
        *(.rodata .rodata.* .srodata .srodata.*)
    } > FW

    .data : {
        *(.data .data.* .sdata .sdata.*)
    } > FW

    .bss : {
        . = ALIGN(8);
        _bss_start = .;
        *(.bss .bss.* .sbss .sbss.* COMMON)
        . = ALIGN(8);
        _bss_end = .;
    } > FW

    _fw_end = .;
    _fw_stack_top = ORIGIN(FW) + LENGTH(FW);
    _fw_region_size = LENGTH(FW); /* what the host reserves; read back out of the ELF */

    /DISCARD/ : {
        *(.eh_frame .note .comment .riscv.attributes)
    }
}

ASSERT(_fw_end + STACK_SIZE <= _fw_stack_top, "firmware overflows its reserved region")
