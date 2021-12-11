//mem.c
//User memory space tracking
//Bryan E. Topp <betopp@betopp.com> 2021

#include "mem.h"
#include "kassert.h"
#include "m_frame.h"
#include "m_uspc.h"
#include <errno.h>

void mem_clear(mem_t *mem)
{
	size_t pagesize = m_frame_size();
	
	for(int ss = 0; ss < MEM_SEG_MAX; ss++)
	{
		uintptr_t start = mem->segs[ss].vaddr;
		uintptr_t end = start + mem->segs[ss].size;

		if(end > start)
		{
			KASSERT(start % pagesize == 0);
			KASSERT(end % pagesize == 0);
			KASSERT(mem->uspc != 0);
			
			for(uintptr_t pp = start; pp < end; pp += pagesize)
			{
				uintptr_t frame = m_uspc_get(mem->uspc, pp);
				KASSERT(frame != 0);
				m_uspc_set(mem->uspc, pp, 0, 0);
				m_frame_free(frame);
			}
		}
			
		mem->segs[ss].vaddr = 0;
		mem->segs[ss].size = 0;
		mem->segs[ss].prot = 0;
	}
		
	if(mem->uspc != 0)
	{
		m_uspc_delete(mem->uspc);
		mem->uspc = 0;
	}
}

int mem_add(mem_t *mem, uintptr_t vaddr, size_t size, int prot)
{
	//Location and size must be page-aligned
	size_t pagesize = m_frame_size();
	if(vaddr % pagesize != 0)
		return -EINVAL;
	if(size % pagesize != 0)
		return -EINVAL;
	
	//Find place to store the segment
	mem_seg_t *sptr = NULL;
	for(int ss = 0; ss < MEM_SEG_MAX; ss++)
	{
		if(mem->segs[ss].size == 0)
		{
			sptr = &(mem->segs[ss]);
			break;
		}
	}
	if(sptr == NULL)
	{
		//No room for more segment references
		return -EMFILE;
	}
	
	//Make the userspace paging structures, if none exist
	if(mem->uspc == 0)
	{
		mem->uspc = m_uspc_new();
		if(mem->uspc == 0)
		{
			//Failed to allocate paging structures.
			return -ENOMEM;
		}
	}
	
	//Make sure the virtual space is free
	for(uintptr_t pp = vaddr; pp < vaddr + size; pp += pagesize)
	{
		uintptr_t oldframe = m_uspc_get(mem->uspc, pp);
		if(oldframe != 0)
			return -EBUSY;
	}
	
	//Try to fill the requested range
	for(uintptr_t pp = vaddr; pp < vaddr + size; pp += pagesize)
	{
		uintptr_t newframe = m_frame_alloc();
		if(newframe != 0)
		{
			bool mapped = m_uspc_set(mem->uspc, pp, newframe, prot);
			if(mapped)
			{
				//Success - keep going
				continue;
			}
		}
		
		//Failed to allocate or map a frame here. Unwind and fail.
		if(newframe != 0)
		{
			m_frame_free(newframe);
			newframe = 0;
		}
		
		while(pp > vaddr)
		{
			pp -= pagesize;
			uintptr_t unwind = m_uspc_get(mem->uspc, pp);
			KASSERT(unwind != 0);
			m_uspc_set(mem->uspc, pp, 0, 0);
			m_frame_free(unwind);
		}
		
		return -ENOMEM;
	}
	
	//Success
	sptr->vaddr = vaddr;
	sptr->size = size;
	sptr->prot = prot;
	return 0;
}
