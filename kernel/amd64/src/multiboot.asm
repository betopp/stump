;multiboot.asm
;Entry from bootloader
;Bryan E. Topp <betopp@betopp.com> 2021

section .multiboot
bits 32

;Bootloader looks for this to find header
%define MULTIBOOT_MAGIC 0x1BADB002

;Flags provided to bootloader.
;Bit 16 - we have memory info.
%define MULTIBOOT_FLAGS 0x00010000

;Header checksum
%define MULTIBOOT_CHECKSUM (0x100000000 - MULTIBOOT_MAGIC - MULTIBOOT_FLAGS)

;Physical addresses defined in linker script
extern _MULTIBOOT_LOAD_ADDR
extern _MULTIBOOT_LOAD_END_ADDR
extern _MULTIBOOT_BSS_END_ADDR

;Multiboot header, identifying us to the bootloader
global multiboot_header
multiboot_header:
	dd MULTIBOOT_MAGIC
	dd MULTIBOOT_FLAGS
	dd MULTIBOOT_CHECKSUM
	dd (multiboot_header - $$) + _MULTIBOOT_LOAD_ADDR ;Where this header goes
	dd _MULTIBOOT_LOAD_ADDR ;Start-of-loading
	dd _MULTIBOOT_LOAD_END_ADDR ;End-of-loading
	dd _MULTIBOOT_BSS_END_ADDR ;End-of-kernel
	dd (multiboot_entry - $$) + _MULTIBOOT_LOAD_ADDR ;Entry point
	
;Entry point from bootloader - 32-bit protected mode, but with paging off
global multiboot_entry
multiboot_entry:
	
	;Make sure we were loaded by multiboot - crash otherwise
	cmp EAX, 0x2BADB002
	jne multiboot_entry
	
	;Head into setting up our AMD64
	;Make sure this is a relative jump - we're in real-mode and don't have proper symbols available
	clc
	extern cpuinit_entry
	jnc cpuinit_entry
