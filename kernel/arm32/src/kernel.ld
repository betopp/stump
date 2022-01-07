/* kernel.ld
 * Linker script for kernel on 32-bit ARM
 * Bryan E. Topp <betopp@betopp.com> 2021
 */

ENTRY(_start)

SECTIONS {

	/* DRAM location in integratorcp QEMU target */
	_PMEM_START = 0x00000000;
	_PMEM_END = _PMEM_START + (128*1024*1024);

	/*
	Kernel goes physically at 0x00000000/0x80000000 to line up with DRAM region.
	Kernel goes virtually in the top 256MBytes of 32-bit space.
	_KSPACE_BASE is the difference between virtual and physical addresses for the kernel as-linked.
	*/
	_KSPACE_PHYS = _PMEM_START;
	_KSPACE_BASE = 0xF0000000 - _KSPACE_PHYS;

	. = _KSPACE_PHYS + _KSPACE_BASE;
	_KERNEL_START = .;
	.text ALIGN(0x4000) : AT(ADDR(.text) - _KSPACE_BASE)
	{
		*(.boot)
		
		. = ALIGN(0x4000);
		*(.text)
		
		*(.rodata*)
		*(.note*)
	}
	
	.data ALIGN(0x4000) : AT(ADDR(.data) - _KSPACE_BASE)
	{
		. = ALIGN(0x4000);
		*(.data)
	}
	
	/*System image gets linked into kernel*/
	. = ALIGN(0x4000); /*Pad to page boundary so we can free it*/
	_SYSTAR_START = .;
	.systar ALIGN(0x4000) : AT(ADDR(.systar) - _KSPACE_BASE)
	{
		*(.systar*)
		. = ALIGN(0x4000); /*Pad to page boundary so we can free it*/
	}
	_SYSTAR_END = .;

	_BSS_START = .;
	.bss ALIGN(0x4000) : AT(ADDR(.bss) - _KSPACE_BASE)
	{
		*(COMMON)
		
		. = ALIGN(0x4000);
		*(.bss)
		. = ALIGN(0x4000);
	}
	_BSS_END = .;
	
	/*Pad end-of-kernel to page boundary*/
	.endpad ALIGN(0x4000) : AT(ADDR(.endpad) - _KSPACE_BASE)
	{
		_ENDPAD_MARKER = .;
	}

	_KERNEL_END = .;
}
