OUTPUT_ARCH(arm)
ENTRY(_start)
/* ndstool puts its own 0x800-byte secure-area filler at 0x02000000 in front of this binary, so the code is linked at 0x02000800 */
MEMORY { ram : ORIGIN = 0x02000800, LENGTH = 0x00380000 }
SECTIONS {
  .crt0   : { *(.crt0) } > ram
  .text   : { *(.text .text.*) *(.glue_7t) *(.glue_7) . = ALIGN(4); } > ram
  .rodata : { *(.rodata .rodata.*) . = ALIGN(4); } > ram
  .data   : { *(.data .data.*) . = ALIGN(4); } > ram
  __bss_start = .;
  .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) . = ALIGN(4); } > ram
  __bss_end = .;
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.eh_frame*) *(.comment) }
}
