//m_uspc.c
//Userspace management on AMD64
//Bryan E. Topp <betopp@betopp.com> 2021

#include "m_uspc.h"
#include "m_frame.h"
#include "m_panic.h"
#include "pspace.h"
#include "amd64.h"

//Kernel PML4 (top-level paging)
extern uint64_t cpuinit_pml4[];

//Difference between virtual and physical addresses in kernel as-linked, from linker script
extern const uint8_t _KERNEL_VOFFS[];

void m_uspc_range(uintptr_t *start_out, uintptr_t *end_out)
{
	//Start just above the zero-page
	*start_out = 4096;
	
	//End halfway through low-half of 48-bit canonical space.
	//(We might want that other quarter of the space later...)
	*end_out = 0x00003FFFFFFFF000ul;
}

m_uspc_t m_uspc_new(void)
{
	//Allocate a frame for the PML4
	uintptr_t pml4 = m_frame_alloc();
	if(pml4 == 0)
		return 0;
	
	//Zero the new PML4
	pspace_clrframe(pml4);
	
	//Copy the references to the kernel's PDPT and the physical-space PDPT.
	for(int ee = 510; ee < 512; ee++)
	{
		pspace_write(pml4 + (8 * ee), cpuinit_pml4[ee]);
	}
	
	return pml4;
}

void m_uspc_delete(m_uspc_t uspc)
{
	//Traverse the whole paging hierarchy and free all tables.
	//The last level - the pagetables - should all be empty, if we've unmapped everything.
	uint64_t pml4_base = uspc;
	for(int pml4_idx = 0; pml4_idx < 256; pml4_idx++)
	{
		uint64_t pml4e = pspace_read(pml4_base + (8 * pml4_idx));
		if(!(pml4e & 1))
			continue;
		
		uint64_t pdpt_base = pml4e & 0x00FFFFFFFFFFF000ul;
		for(int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++)
		{
			uint64_t pdpte = pspace_read(pdpt_base + (8 * pdpt_idx));
			if(!(pdpte & 1))
				continue;
			
			uint64_t pd_base = pdpte & 0x00FFFFFFFFFFF000ul;
			for(int pd_idx = 0; pd_idx < 512; pd_idx++)
			{
				uint64_t pde = pspace_read(pd_base + (8 * pd_idx));
				if(!(pde & 1))
					continue;
				
				uint64_t pt_base = pde & 0x00FFFFFFFFFFF000ul;
				for(int pt_idx = 0; pt_idx < 512; pt_idx++)
				{
					uint64_t pte = pspace_read(pt_base + (8 * pt_idx));
					if(pte & 1)
						m_panic("m_uspc_delete nonempty");
				}
				
				m_frame_free(pt_base);
			}
			
			m_frame_free(pd_base);
		}
		
		m_frame_free(pdpt_base);
	}
	
	m_frame_free(pml4_base);
}

bool m_uspc_set(m_uspc_t uspc, uintptr_t vaddr, uintptr_t paddr, int prot)
{
	if(vaddr & 0xFFFF000000000FFFul)
		m_panic("m_uspc_set bad vaddr");
	
	if(paddr & 0xFFFul)
		m_panic("m_uspc_set bad paddr");
	
	const uint64_t pml4_base = uspc;
	const uint64_t pml4_idx = (vaddr >> 39) % 512;
	uint64_t pml4e = pspace_read(pml4_base + (8 * pml4_idx));
	if(!(pml4e & 1))
	{
		//PML4e is non-present - need to allocate a new PDPT and put it here.
		uint64_t newpdpt = m_frame_alloc();
		if(newpdpt == 0)
			return false; //No room for PDPT
		
		pspace_clrframe(newpdpt);
		
		pml4e = newpdpt | 0x7; //Present, writable, user-accessible
		pspace_write(pml4_base + (8 * pml4_idx), pml4e);
	}
	
	const uint64_t pdpt_base = pml4e & 0x00FFFFFFFFFFF000ul;
	const uint64_t pdpt_idx = (vaddr >> 30) % 512;
	uint64_t pdpte = pspace_read(pdpt_base + (8 * pdpt_idx));
	if(!(pdpte & 1))
	{
		//PDPTe is non-present - need to allocate a new PD and put it here.
		uint64_t newpd = m_frame_alloc();
		if(newpd == 0)
			return false; //No room for PD
		
		pspace_clrframe(newpd);
		
		pdpte = newpd | 0x7; //Present, writable, user-accessible
		pspace_write(pdpt_base + (8 * pdpt_idx), pdpte);
	}
	
	const uint64_t pd_base = pdpte & 0x00FFFFFFFFFFF000ul;
	const uint64_t pd_idx = (vaddr >> 21) % 512;
	uint64_t pde = pspace_read(pd_base + (8 * pd_idx));
	if(!(pde & 1))
	{
		//PDe is non-present - need to allocate a new PT and put it here.
		uint64_t newpt = m_frame_alloc();
		if(newpt == 0)
			return false; //No room for PT
		
		pspace_clrframe(newpt);
		
		pde = newpt | 0x7; //Present, writable, user-accessible
		pspace_write(pd_base + (8 * pd_idx), pde);
	}
	
	const uint64_t pt_base = pde & 0x00FFFFFFFFFFF000ul;
	const uint64_t pt_idx = (vaddr >> 12) % 512;
	uint64_t pte = pspace_read(pt_base + (8 * pt_idx));
	if(!(pte & 1))
	{
		//PTE not already present. Allow putting a page here.
		if(paddr == 0)
		{
			pte = 0;
		}
		else
		{
			pte = paddr;
			
			//Present
			pte |= 0x1; 
			
			//Writable
			if(prot & M_USPC_PROT_W)
				pte |= 0x2; //RW
			
			//No-execute
			if(!(prot & M_USPC_PROT_X))
				pte |= 0x8000000000000000ul; //NX
			
			//Any access - make usermode-visible
			if(prot != 0)
				pte |= 0x4; //US
		}			
	}
	else
	{
		//PTE is already present. Only allow unmapping.
		if(paddr == 0)
		{
			pte = 0;
		}
		else
		{
			m_panic("m_uspc_set reassign");
		}
	}
	
	pspace_write(pt_base + (8 * pt_idx), pte);
	return true;
}

uintptr_t m_uspc_get(m_uspc_t uspc, uintptr_t vaddr)
{
	if(vaddr & 0xFFFF000000000FFFul)
		m_panic("m_uspc_get bad vaddr");
	
	const uint64_t pml4_base = uspc;
	const uint64_t pml4_idx = (vaddr >> 39) % 512;
	const uint64_t pml4e = pspace_read(pml4_base + (8 * pml4_idx));
	if(!(pml4e & 1))
		return 0; //PML4e not present
	
	if(!(pml4e & 4))
		m_panic("m_uspc_get pml4e not user");
	
	const uint64_t pdpt_base = pml4e & 0x00FFFFFFFFFFF000ul;
	const uint64_t pdpt_idx = (vaddr >> 30) % 512;
	const uint64_t pdpte = pspace_read(pdpt_base + (8 * pdpt_idx));
	if(!(pdpte & 1))
		return 0; //PDPTe not present
	
	if(!(pdpte & 4))
		m_panic("m_uspc_get pdpte not user");
	
	const uint64_t pd_base = pdpte & 0x00FFFFFFFFFFF000ul;
	const uint64_t pd_idx = (vaddr >> 21) % 512;
	const uint64_t pde = pspace_read(pd_base + (8 * pd_idx));
	if(!(pde & 1))
		return 0; //PDe not present
	
	if(!(pde & 4))
		m_panic("m_uspc_get pde not user");
	
	const uint64_t pt_base = pde & 0x00FFFFFFFFFFF000ul;
	const uint64_t pt_idx = (vaddr >> 12) % 512;
	const uint64_t pte = pspace_read(pt_base + (8 * pt_idx));
	if(!(pte & 1))
		return 0; //PTe not present
	
	//(Allow PTE to be non-user-accessible if they asked to not be able to read it.)
	
	return pte & 0x00FFFFFFFFFFF000ul;
}

m_uspc_t m_uspc_current()
{
	uintptr_t cr3 = getcr3();
	
	if(cr3 == (uintptr_t)cpuinit_pml4 - (uintptr_t)_KERNEL_VOFFS)
		return 0; //Indicates "no userspace"
	
	return cr3;
}

void m_uspc_activate(m_uspc_t uspc)
{
	if(uspc & 0xFFF)
		m_panic("m_uspc_activate misalign");
	
	if(uspc == 0) //Indicates "no userspace"
		setcr3((uintptr_t)cpuinit_pml4 - (uintptr_t)_KERNEL_VOFFS);
	else
		setcr3(uspc);
}

