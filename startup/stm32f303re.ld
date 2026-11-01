ENTRY(Reset_Handler)
_estack = ORIGIN(RAM) + LENGTH(RAM);   /* stack starts at top of SRAM */

_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x400;
MEMORY {
FLASH (rx)  : ORIGIN  = 0x08000000, LENGTH = 512K
RAM   (xrw) : ORIGIN  = 0x20000000, LENGTH = 64K
CCMRAM (xrw) : ORIGIN =  0x10000000, LENGTH = 16K
} /* Memory setup */

SECTIONS {
.isr_vector : {
. = ALIGN(4);
KEEP(*(.isr_vector))
} >FLASH

.text : {
. = ALIGN(4);
*(.text) *(.text*)
} >FLASH

.rodata : {
. = ALIGN(4);
*(.rodata) *(.rodata*)
} >FLASH

.ARM.exid : {
. = ALIGN(4);
*(.ARM.exidx*)
} >FLASH

 _sidata = LOADADDR(.data);
 .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data) *(.data*)
    *(.RamFunc) *(.RamFunc*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss) *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } >RAM

._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
