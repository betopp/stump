/* kernel.ld */
/* GNU linker script for building 64-bit kernel */
/* Bryan E. Topp <betopp@betopp.com> 2021 */

ENTRY(multiboot_entry)

SECTIONS
{
	/* Difference between physical and virtual addresses as-linked. */
	/* Put the kernel in the top 2GBytes so we can use negative 32-bit offsets. */
	_KERNEL_VOFFS = 0xFFFFFFFF80000000;
	
	/* Put the kernel at +2MBytes physically, to get away from BIOS/DOS crap. */
	. = _KERNEL_VOFFS + 0x200000;
	
	/* Start-of-loading for multiboot */
	_MULTIBOOT_LOAD_ADDR = . - _KERNEL_VOFFS;
	
	.boot ALIGN(0x1000) : AT(ADDR(.boot) - _KERNEL_VOFFS)
	{	
		/* Put multiboot in its own place up front, so bootloader can find it*/
		*(.multiboot*)
	}
	
	.text ALIGN(0x1000) : AT(ADDR(.text) - _KERNEL_VOFFS)
	{	
		*(.eh_frame*)
		*(.text*)
	}
	
	.rodata ALIGN(0x1000) : AT(ADDR(.rodata) - _KERNEL_VOFFS)
	{	
		*(.rodata)
		
		/* System TAR needs to be page-aligned so we can free it after unpacking */
		. = ALIGN(0x1000);
		_SYSTAR_START = .;
		*(.systar*)
		. = ALIGN(0x1000);
		_SYSTAR_END = .;
	}
	
	.data ALIGN(0x1000) : AT(ADDR(.data) - _KERNEL_VOFFS)
	{	
		*(.data)
		
		. += 1;
		. = ALIGN(0x1000);
	}
	
	/* End-of-loading for multiboot */
	_MULTIBOOT_LOAD_END_ADDR = . - _KERNEL_VOFFS;
	
	.bss ALIGN(0x1000) : AT(ADDR(.bss) - _KERNEL_VOFFS)
	{
		*(.bss)
		*(COMMON)
	}
	
	/*Pad end-of-kernel to page boundary*/
	.endpad ALIGN(0x1000) : AT(ADDR(.endpad) - _KERNEL_VOFFS)
	{
	
	}
	
	/* End-of-zero for multiboot */
	_MULTIBOOT_BSS_END_ADDR = . - _KERNEL_VOFFS;
}
