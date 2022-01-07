//m_kspc.c
//Kernel space management on 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

#include "m_kspc.h"
#include "m_panic.h"
#include "arm32.h"

//256 page tables of 256 entries each, giving us 256 MBytes of kernel space.
//These are referenced contiguously at the top of the kernel translation table.
extern uint32_t _kpagetables[65536];

//Defined in linker script - physical address after kernel as-linked.
extern uint8_t _KERNEL_END[];

void m_kspc_range(uintptr_t *start_out, uintptr_t *end_out)
{
	//Allocatable range is just after kernel as-linked...
	*start_out = (uintptr_t)_KERNEL_END;
	
	//...until vector table at end of address space
	*end_out   = 0xFFFF0000;
}

bool m_kspc_set(uintptr_t vaddr, uintptr_t paddr)
{
	if(paddr % 16384)
		m_panic("m_kspc_set p misalign");
	if(vaddr % 16384)
		m_panic("m_kspc_set v misalign");
	if(vaddr <  0xF0000000ul)
		m_panic("m_kspc_set v < bottom");
	if(vaddr >= 0xFFFF0000ul)
		m_panic("m_kspc_set v >= top");
	
	//We'll need to fill 4 adjacent entries to map the 16KByte region we want
	uint32_t idx = (vaddr - 0xF0000000ul) / 4096;
	for(uint32_t ii = idx; ii < idx + 4; ii++)
	{
		_kpagetables[ii] = paddr;
		_kpagetables[ii] |= 0xE; //writeback cacheable small page
		_kpagetables[ii] |= 0x550; //Kernel-only writable (all 4 subpages)
		_arm_invlpg((ii * 4096ul) + 0xF0000000ul);
		paddr += 4096;
	}
	
	return true;
}

uintptr_t m_kspc_get(uintptr_t vaddr)
{
	if(vaddr % 16384)
		m_panic("m_kspc_get v misalign");
	if(vaddr <  0xF0000000ul)
		m_panic("m_kspc_get v < bottom");
	if(vaddr >= 0xFFFF0000ul)
		m_panic("m_kspc_get v >= top");	
	
	uint32_t idx = (vaddr - 0xF0000000ul) / 4096;
	return _kpagetables[idx] & 0xFFFFF000ul;
}
