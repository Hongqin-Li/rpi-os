/* KERNBASE = 0xFFFF000000000000; */
KERNBASE = 0;
KERNLINK = KERNBASE + 80000;

ENTRY(_start)
/* Since we are actually booting a binary image, the entry
 * is completely irrelevant, but it has to be there in the
 * elf file we build as intermediate file. */

SECTIONS
{
  . = KERNLINK;

  _kernel_start = .;

  .text ALIGN (4K) : AT (ADDR(.text) - KERNBASE) {
    KEEP(*(.text.boot))
    *(.text)
  }
  PROVIDE(etext = .);

  .rodata : AT (ADDR(.rodata) - KERNBASE) { *(.rodata) }

  . = ALIGN(4K);

  PROVIDE(data = .);
  .data : AT (ADDR(.data) - KERNBASE) { *(.data) }
  PROVIDE(edata = .);
  .bss : AT (ADDR(.bss) - KERNBASE) { *(.bss) }
  PROVIDE(end = .);
}
