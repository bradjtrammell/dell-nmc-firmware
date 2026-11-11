/* OFFLINE-ONLY ARM7 application linker layout. Address was observed in
 * Dell's image header; it is NOT a verified H910P RAM map. */
ENTRY(_start)
SECTIONS {
    . = 0x08004000;
    __image_start = .;
    .text : ALIGN(4) { KEEP(*(.vectors)) *(.text.boot) *(.text .text.*) }
    .rodata : ALIGN(4) { *(.rodata .rodata.*) }
    .data : ALIGN(4) { *(.data .data.*) }
    __bss_start = .;
    .bss (NOLOAD) : ALIGN(4) { *(.bss .bss.*) *(COMMON) }
    __bss_end = ALIGN(4);
    . = __bss_end;
    .stack (NOLOAD) : ALIGN(8) { . = . + 8192; }
    __stack_top = .;
    __image_end = .;
    ASSERT((__image_end - __image_start) < 0x00100000, "Offline ARM image exceeds 1 MiB research bound")
    /DISCARD/ : { *(.comment) *(.note.*) *(.ARM.exidx*) *(.ARM.attributes) }
}
