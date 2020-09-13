KERNBASE = 0xFFFF000000000000;
KERNLINK = 0xFFFF000000080000;

ENTRY(_start)
/* Since we are actually booting a binary image, the entry
 * is completely irrelevant, but it has to be there in the
 * elf file we build as intermediate file. */

SECTIONS
{
  . = KERNLINK;

  _kernel_start = .;

  .text ALIGN (4K) : {
    KEEP(*(.text.boot))
    *(.text)
  }
  PROVIDE(etext = .);
  .rodata : { *(.rodata) }

  . = ALIGN(4K);

  PROVIDE(data = .);
  .data : { *(.data) }
  PROVIDE(edata = .);
  .bss : { *(.bss) }
  PROVIDE(end = .);
}
