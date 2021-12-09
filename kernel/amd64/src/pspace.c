//pspace.c
//Physical space access on AMD64
//Bryan E. Topp <betopp@betopp.com> 2021

#include "pspace.h"
#include "m_panic.h"

//In cpuinit.asm, we make a PDPT (512GBytes of virtual space) filled entirely with 1GByte huge-pages.
//We use this to access the first 512GBytes of physical space in a contiguous region just below kernel-space.
#define PSPACE_SIZE (4096ul*512*512*512) //One PDPT
#define PSPACE_BASE (-2l*PSPACE_SIZE) //Second-to-top PDPT in each PML4 - top PDPT is the kernel-space

uint64_t pspace_read(uint64_t addr)
{
	if(addr > PSPACE_SIZE)
		m_panic("pspace_read bad addr");
	
	return *((uint64_t*)(PSPACE_BASE + addr));
}

void pspace_write(uint64_t addr, uint64_t data)
{
	if(addr > PSPACE_SIZE)
		m_panic("pspace_write bad addr");
	
	*((uint64_t*)(PSPACE_BASE + addr)) = data;
}

void pspace_clrframe(uint64_t addr)
{
	if(addr > PSPACE_SIZE)
		m_panic("pspace_clrframe bad addr");
	if(addr & 0xFFFul)
		m_panic("pspace_clrframe misalign");
	
	for(int pp = 0; pp < 512; pp++)
	{
		((uint64_t*)(PSPACE_BASE + addr))[pp] = 0;
	}
}
