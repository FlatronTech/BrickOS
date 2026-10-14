SECTIONS
{
    . = 0x10000;
    .text : { *(.text.start) *(.text*) }
    .rodata : { *(.rodata*) *(.rdata*) }
    .data : { *(.data*) }
    ___bss_start = .;
    __bss_start = .;
    .bss : { *(.bss*) *(COMMON) }
    ___bss_end = .;
    __bss_end = .;
    /DISCARD/ : { *(.eh_frame*) *(.comment) *(.debug*) *(.reloc*) }
}