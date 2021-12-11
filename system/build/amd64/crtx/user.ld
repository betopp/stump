/* user.ld
 * Linker script for userland on AMD64
 * Bryan E. Topp <betopp@betopp.com> 2021
 */

ENTRY(_crt_entry)

SECTIONS {

	. = 0x1000000 + SIZEOF_HEADERS;

	.text ALIGN(0x1000) :
	{
		*(.header)
		*(.text*)
		*(.note*)
		*(.init.i)
		*(.init)
		*(.init.n)
		*(.fini.i)
		*(.fini)
		*(.fini.n)
		. = ALIGN(0x1000);
		_TEXT_END = .;
	}

	.rodata ALIGN(0x1000) :
	{
		*(.rodata*)
		*(.eh_frame*)
		. = ALIGN(0x1000);
		_RODATA_END = .;
	}
	
	.data ALIGN(0x1000) :
	{
		*(.data*)
		. = ALIGN(0x1000);
		_DATA_END = .;
	}
	
	.bss ALIGN(0x1000) :
	{
		*(COMMON)
		*(.bss*)
		. = ALIGN(0x1000);
		_BSS_END = .;
	}
}
