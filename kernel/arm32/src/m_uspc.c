//m_uspc.c
//Userspace management on 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

#include "m_uspc.h"
#include "m_kspc.h"
#include "m_frame.h"
#include "m_panic.h"
#include "arm32.h"
#include <string.h>

//Top-level translation table containing kernel mapping.
//Used as template for user translation tables.
extern uint32_t _ktrantable[4096];

//Window where we map pagetables while working on them
__attribute__((aligned(16384))) uint32_t _uspc_window[4096];

void m_uspc_range(uintptr_t *start_out, uintptr_t *end_out)
{
	*start_out = 16384;
	*end_out = 0x80000000ul;
}

m_uspc_t m_uspc_new(void)
{
	uintptr_t frame = m_frame_alloc();
	if(frame == 0)
		return 0;
	
	m_kspc_set((uintptr_t)_uspc_window, frame);
	memcpy(_uspc_window, _ktrantable, sizeof(_uspc_window));
	if(_uspc_window[0])
		m_panic("m_uspc_new corrupt ktrantable");
	
	return frame;
}

void m_uspc_delete(m_uspc_t uspc)
{
	//Free all pagetables referenced by the translation table.
	//Note that each one is actually 4 entries - but treated as one 16KByte frame.
	//Only free the low half (userspace)
	m_kspc_set((uintptr_t)_uspc_window, uspc);
	for(int pp = 0; pp < 2048; pp += 4)
	{
		if(_uspc_window[pp])
		{
			m_frame_free(_uspc_window[pp] & 0xFFFFF000ul);
		}
	}
	
	//Free the translation table itself.
	m_frame_free(uspc);
}

//Changes the mapping of a page in userspace.
//Returns true if the mapping was made; false otherwise (probably: out of physical RAM).
bool m_uspc_set(m_uspc_t uspc, uintptr_t vaddr, uintptr_t paddr, int prot)
{
	if(vaddr % 16384)
		m_panic("m_uspc_set misalign v");
	
	//Examine the translation table and see if we need to allocate a set of pagetables.
	//We allocate 16KByte frames, which are 16 coarse page tables, corresponding to 16 MBytes of space.
	m_kspc_set((uintptr_t)_uspc_window, uspc);
	uint32_t tt_idx = vaddr / 1048576;
	if(_uspc_window[tt_idx] == 0)
	{
		//No pagetable here yet. Allocate one.
		uintptr_t pt_frame = m_frame_alloc();
		if(pt_frame == 0)
		{
			//No pagetable at this location, and no room for one. Out of memory.
			return false;
		}
		
		m_kspc_set((uintptr_t)_uspc_window, pt_frame);
		memset(_uspc_window, 0, sizeof(_uspc_window));
		m_kspc_set((uintptr_t)_uspc_window, uspc);
		
		//Got a new pagetable - put it in the translation table.
		//Our new 16KByte pagetable frame is actually 16 Coarse Page Tables.
		//Reference them from 16 descriptors in the translation table.
		uint32_t tt_idx_start = tt_idx - (tt_idx % 16);
		uint32_t tt_idx_end = tt_idx_start + 16;
		for(uint32_t tt = tt_idx_start; tt < tt_idx_end; tt++)
		{
			if(_uspc_window[tt] != 0)
				m_panic("corrupt tt");
			
			_uspc_window[tt] = pt_frame | 0x11;
			pt_frame += 1024;
		}
	}
	
	uint32_t pt_addr = _uspc_window[tt_idx - (tt_idx % 16)];
	if(pt_addr == 0)
		m_panic("corrupt tt 2");
	
	pt_addr &= 0xFFFFF000ul;
	
	//Set the entry in the pagetable
	m_kspc_set((uintptr_t)_uspc_window, pt_addr);
	
	uint32_t pt_idx = (vaddr / 4096) % 4096;
	
	uint32_t entry = paddr;
	if(prot & M_USPC_PROT_W)
		entry |= 0xFF0; //User writable (all 4 subpages)
	else
		entry |= 0xAA0; //Read-only but still user accessible (all 4 subpages)
	
	//Fill in 4 pagetable entries for the 16KByte frame
	for(int ff = 0; ff < 4; ff++)
	{
		_uspc_window[pt_idx+ff] = entry + (4096*ff);
		_arm_invlpg((vaddr & 0xFFFFC000) + (4096*ff));
	}
	
	return true;
}

uintptr_t m_uspc_get(m_uspc_t uspc, uintptr_t vaddr)
{
	if(vaddr % 16384)
		m_panic("m_uspc_get misalign v");
	
	//Examine the translation table and see if we need to allocate a set of pagetables.
	//We allocate 16KByte frames, which are 16 coarse page tables, corresponding to 16 MBytes of space.
	m_kspc_set((uintptr_t)_uspc_window, uspc);
	uint32_t tt_idx = vaddr / 1048576;
	if(_uspc_window[tt_idx] == 0)
	{
		//No pagetable here
		return 0;
	}
	
	uint32_t pt_addr = _uspc_window[tt_idx - (tt_idx % 16)];
	if(pt_addr == 0)
		m_panic("corrupt tt 2");
	
	pt_addr &= 0xFFFFF000ul;
	
	//Find the entry in the pagetable
	m_kspc_set((uintptr_t)_uspc_window, pt_addr);
	
	uint32_t pt_idx = (vaddr / 4096) % 4096;
	return _uspc_window[pt_idx] & 0xFFFFC000ul;	
}

