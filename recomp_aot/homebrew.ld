/* Fixed addresses for our test-only ARM64 ELF. A real Switch title uses a proper linker/loader. */
ENTRY(demo_entry)
SECTIONS {
 . = 0x40;
 .text : { KEEP(*(.text.demo_entry)) *(.text .text.*) }
 . = 0x1000;
 .rodata : { *(.rodata .rodata.*) }
 . = 0x2000;
 .data : { *(.data .data.*) }
 . = 0x2100;
 .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) }
 /DISCARD/ : { *(.comment*) *(.note*) *(.eh_frame*) *(.ARM.*) }
}
