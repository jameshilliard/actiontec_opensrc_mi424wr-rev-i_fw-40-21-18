/*
 *  linux/arch/arm/boot/bootp/bootp.lds
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
OUTPUT_ARCH(arm)
ENTRY(_start)
SECTIONS
{
  . = 0;
  _text = .;
  .text : {
   _stext = .;
   _start = .;
   *(.bootstrap)
   *(.start)
   kernel_start = .;
   kernel.o
   kernel_len = ABSOLUTE(.) - kernel_start;
   . = ALIGN(32);
   *(.text)
   *(.rodata)
   . = ALIGN(32);
   _etext = .;
  }
  
  .bss   0xf0000000 :
  { 
  	_bstart = . ; 
	*(.*bss) *(COMMON) ; 
	_bend = .; 
  }

  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment 0 : { *(.comment) }
}
