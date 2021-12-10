/* user.ld
 * Linker script for userland on AMD64
 * Bryan E. Topp <betopp@betopp.com> 2021
 */

ENTRY(_crt_entry)

SECTIONS {

	. = 0x1000;

	.text ALIGN(0x1000) :
	{
		*(.header)
		*(.text)
		*(.note*)
		*(.init.i)
		*(.init)
		*(.init.n)
		*(.fini.i)
		*(.fini)
		*(.fini.n)
		_TEXT_END = .;
	}

	.rodata ALIGN(0x1000) :
	{
		*(.rodata*)
		*(.eh_frame*)
		_RODATA_END = .;
	}
	
	.data ALIGN(0x1000) :
	{
		*(.data)
		_DATA_END = .;
	}
	
	.bss ALIGN(0x1000) :
	{
		*(COMMON)
		*(.bss)
		_BSS_END = .;
	}
}
