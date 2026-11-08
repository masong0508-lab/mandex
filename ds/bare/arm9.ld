OUTPUT_ARCH(arm)
ENTRY(_start)
MEMORY { ram : ORIGIN = 0x02000000, LENGTH = 0x00380000 }
SECTIONS {
  .secure : { BYTE(0); . = 0x800; } > ram
  .crt0   : { *(.crt0) } > ram
  .text   : { *(.text .text.*) *(.glue_7t) *(.glue_7) . = ALIGN(4); } > ram
  .rodata : { *(.rodata .rodata.*) . = ALIGN(4); } > ram
  .data   : { *(.data .data.*) . = ALIGN(4); } > ram
  __bss_start = .;
  .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) . = ALIGN(4); } > ram
  __bss_end = .;
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.eh_frame*) *(.comment) }
}
