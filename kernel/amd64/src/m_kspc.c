//m_kspc.c
//Kernel space management, AMD64 implementation
//Bryan E. Topp <betopp@betopp.com> 2021

#include "m_kspc.h"
#include "m_panic.h"
#include "m_spl.h"
#include "m_frame.h"
#include "pspace.h"

//PDPT making up kernel-space, from cpuinit.asm
extern uint64_t cpuinit_pdpt[];

//Spinlock protecting kernel-space
static m_spl_t m_kspc_spl;

static bool m_kspc_set_locked(uintptr_t vaddr, uintptr_t paddr)
{
	//Our kernel lives entirely in the top PML4 entry.
	//So make sure the high bits are all 1, placing us in the highest PML4e (highest PDPT).
	if( (vaddr & 0xFFFFFF8000000000ul) != 0xFFFFFF8000000000ul )
		m_panic("m_kspc_set outside top PML4e");
	
	//Walk down the paging structures and make sure they're all allocated
	uint64_t pdpt_idx = (vaddr & 0x0000007FC0000000ul) >> 30;
	uint64_t pdpte = cpuinit_pdpt[pdpt_idx]; //Kernel PDPT is allocated at boot
	if(!(pdpte & 1))
	{
		//No Page Directory referenced by this Page Directory Pointer Table entry.
		//Should be 0.
		if(pdpte != 0)
			m_panic("m_kspc_set corrupt pdpt");
		
		uint64_t newpd = m_frame_alloc();
		if(newpd == 0)
		{
			if(paddr != 0)
				return false; //PDPTe is empty, but we can't allocate a PD to put there.
			else
				return true; //PDPTe is already empty and we want it to be
		}
		
		pspace_clrframe(newpd);
		pdpte = newpd | 0x3;
		cpuinit_pdpt[pdpt_idx] = pdpte;
	}
	
	uint64_t pd_addr = pdpte & 0x00FFFFFFFFFFF000ul;
	uint64_t pd_idx = (vaddr & 0x000000003FE00000ul) >> 21;
	uint64_t pde = pspace_read(pd_addr + (8 * pd_idx));
	if(!(pde & 1))
	{
		//No Page Table referenced by this Page Directory entry.
		//Should be 0.
		if(pde != 0)
			m_panic("m_kspc_set corrupt pd");
		
		uint64_t newpt = m_frame_alloc();
		if(newpt == 0)
		{
			if(paddr != 0)
				return false; //PDe is empty, but we can't allocate a PT to put there.
			else
				return true; //PDe is already empty and we want it to be
		}
		
		pspace_clrframe(newpt);
		pde = newpt | 0x3;
		pspace_write(pd_addr + (8 * pd_idx), pde);
	}
	
	uint64_t pt_addr = pde & 0x00FFFFFFFFFFF000ul;
	uint64_t pt_idx = (vaddr & 0x00000000001FF000ul) >> 12;
	uint64_t pte = pspace_read(pt_addr + (8 * pt_idx));
	if(!(pte & 1))
	{
		//No page referenced by this page directory entry.
		//Should be 0.
		if(pte != 0)
			m_panic("m_kspc_set corrupt pt");
		
		if(paddr != 0)
			pte = paddr | 0x3;
		
		pspace_write(pt_addr + (8 * pt_idx), pte);
		return true;
	}
	else
	{
		//Page already referenced - only allow overwriting with 0.
		if(paddr != 0)
			m_panic("m_kspc_set rewriting");
		
		pspace_write(pt_addr + (8 * pt_idx), 0);
		return true;
	}
}

static uintptr_t m_kspc_get_locked(uintptr_t vaddr)
{
	//Our kernel lives entirely in the top PML4 entry.
	//So make sure the high bits are all 1, placing us in the highest PML4e (highest PDPT).
	if( (vaddr & 0xFFFFFF8000000000ul) != 0xFFFFFF8000000000ul )
		m_panic("m_kspc_get outside top PML4e");
	
	//Walk down the paging structures and make sure they're all allocated
	uint64_t pdpt_idx = (vaddr & 0x0000007FC0000000ul) >> 30;
	uint64_t pdpte = cpuinit_pdpt[pdpt_idx]; //Kernel PDPT is allocated at boot
	if(!(pdpte & 1))
	{
		//No Page Directory referenced by this Page Directory Pointer Table entry.
		//Should be 0.
		if(pdpte != 0)
			m_panic("m_kspc_set corrupt pdpt");
		
		return 0;
	}
	
	uint64_t pd_addr = pdpte & 0x00FFFFFFFFFFF000ul;
	uint64_t pd_idx = (vaddr & 0x000000003FE00000ul) >> 21;
	uint64_t pde = pspace_read(pd_addr + (8 * pd_idx));
	if(!(pde & 1))
	{
		//No Page Table referenced by this Page Directory entry.
		//Should be 0.
		if(pde != 0)
			m_panic("m_kspc_set corrupt pd");
		
		return 0;
	}
	
	uint64_t pt_addr = pde & 0x00FFFFFFFFFFF000ul;
	uint64_t pt_idx = (vaddr & 0x00000000001FF000ul) >> 12;
	uint64_t pte = pspace_read(pt_addr + (8 * pt_idx));
	if(!(pte & 1))
	{
		//No page referenced by this page directory entry.
		//Should be 0.
		if(pte != 0)
			m_panic("m_kspc_set corrupt pt");
		
		return 0;
	}
	else
	{
		//Page referenced
		return pte & 0x00FFFFFFFFFFF000ul;
	}
}

void m_kspc_range(uintptr_t *start_out, uintptr_t *end_out)
{
	//Kernel as-linked goes in the (-2GByte, 0) range.
	//We reference a single kernel PDPT from the top of every user-space PML4.
	//So we can place dynamic allocation anywhere from (-512GByte, -2GByte).
	//Avoid the first and last gigabyte that we might use, though, because sometimes those entries are involved in identity-mapping.
	*start_out = 0xFFFFFF8000000000ul + (1024ul*1024*1024);
	*end_out   = 0xFFFFFFFF80000000ul - (1024ul*1024*1024);
}

bool m_kspc_set(uintptr_t vaddr, uintptr_t paddr)
{
	m_spl_acq(&m_kspc_spl);
	bool retval = m_kspc_set_locked(vaddr, paddr);
	m_spl_rel(&m_kspc_spl);
	return retval;
}

uintptr_t m_kspc_get(uintptr_t vaddr)
{
	m_spl_acq(&m_kspc_spl);
	uintptr_t retval = m_kspc_get_locked(vaddr);
	m_spl_rel(&m_kspc_spl);
	return retval;
}
