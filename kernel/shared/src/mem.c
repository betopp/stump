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
		size += pagesize - (size % pagesize);
	
	//Find place to store the segment
	mem_seg_t *sptr = NULL;
	int sptr_idx = -1;
	for(int ss = 0; ss < MEM_SEG_MAX; ss++)
	{
		if(mem->segs[ss].size == 0)
		{
			sptr = &(mem->segs[ss]);
			sptr_idx = ss;
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
	
	//Success. Insert into the free index.
	sptr->vaddr = vaddr;
	sptr->size = size;
	sptr->prot = prot;
	
	//Bubble-sort with the other indexes, to keep them in-order.
	while(1)
	{
		if(sptr_idx > 0)
		{
			if((mem->segs[sptr_idx-1].size == 0) || (mem->segs[sptr_idx].vaddr < mem->segs[sptr_idx-1].vaddr))
			{
				mem_seg_t temp = mem->segs[sptr_idx];
				mem->segs[sptr_idx] = mem->segs[sptr_idx - 1];
				mem->segs[sptr_idx - 1] = temp;
				sptr_idx--;
				continue;
			}
		}
		
		if(sptr_idx < MEM_SEG_MAX - 1)
		{
			if((mem->segs[sptr_idx+1].size != 0) && (mem->segs[sptr_idx].vaddr > mem->segs[sptr_idx+1].vaddr))
			{
				mem_seg_t temp = mem->segs[sptr_idx];
				mem->segs[sptr_idx] = mem->segs[sptr_idx + 1];
				mem->segs[sptr_idx + 1] = temp;
				sptr_idx++;
				continue;
			}
		}
		
		break;
	}
	
	return 0;
}

intptr_t mem_avail(mem_t *mptr, uintptr_t around, size_t size)
{
	if(size <= 0)
		return EINVAL;
	
	size_t pagesize = m_frame_size();
	if(size % pagesize != 0)
		size += pagesize - (size % pagesize);
	
	//Get overall bounds of userspace
	uintptr_t uspc_start = 0;
	uintptr_t uspc_end = 0;
	m_uspc_range(&uspc_start, &uspc_end);
	
	KASSERT((size % pagesize) == 0);
	KASSERT((uspc_start % pagesize) == 0);
	KASSERT((uspc_end % pagesize) == 0);
	
	//Segments are stored in-order.
	//This implies that the free spaces can be found in the gaps between them as stored.
	//Search the free regions for a region of at least the given size, closest to the given address.
	
	//If there's nothing at all mapped, then we have a trivial problem
	if((mptr->segs[0].vaddr + mptr->segs[0].size) == 0)
	{
		if( (around >= uspc_start) && ((around + size) <= uspc_end) )
			return around;
		else if(size <= uspc_end - uspc_start)
			return uspc_start;
		else
			return 0;
	}
	
	//Okay, we have at least one segment mapped already.
	//There's more than one place we might put this. Search for the area closest to the requested address.
	uintptr_t best_start = 0;
	uintptr_t best_diff = ~0ul;
	
	//Consider the gaps around each segment
	for(int gg = 0; gg < MEM_SEG_MAX + 1; gg++)
	{
		//Gap gg is the space between memory segments gg-1 and gg
		//If gg is 0 then it's the space from the beginning to segment gg (the first segment)
		//If gg is MEM_SEG_MAX then it's the space from segment gg-1 (the last segment) to the end
		uintptr_t gap_start = (gg == 0) ? uspc_start : ((mptr->segs[gg-1].vaddr) + (mptr->segs[gg-1].size));
		uintptr_t gap_end = ( (gg == MEM_SEG_MAX) || (mptr->segs[gg].size == 0) ) ? uspc_end : (mptr->segs[gg].vaddr);
		
		//Check if the gap is big enough for the proposed region at all
		if(gap_end - gap_start >= size)
		{
			//See what placement would be closest to the proposed address
			uintptr_t best_start_in_gap = 0;
			if(around < gap_start)
			{
				//Wanted an address before the gap - closest we'll get is the beginning
				best_start_in_gap = gap_start;
			}
			else if(around > gap_end - size)
			{
				//Wanted a range that ends after the gap - closest we'll get is the end
				best_start_in_gap = gap_end - size;
			}
			else
			{
				//Can satisfy exactly the request in this gap
				best_start_in_gap = around;
			}
			
			uintptr_t diff = (best_start_in_gap > around) ? (best_start_in_gap - around) : (around - best_start_in_gap);
			if(diff < best_diff)
			{
				best_start = best_start_in_gap;
				best_diff = diff;
			}
		}
		
		if(mptr->segs[gg].size == 0)
			break;
	}
	
	if(best_start == 0)
		return -ENOMEM;
	
	return (intptr_t)best_start;
}

int mem_copy(mem_t *dst, const mem_t *src)
{
	size_t pagesize = m_frame_size();
	
	for(int ss = 0; ss < MEM_SEG_MAX; ss++)
	{
		const mem_seg_t *sptr = &(src->segs[ss]);
		if(sptr->size <= 0)
			continue;
		
		int alloc_err = mem_add(dst, sptr->vaddr, sptr->size, sptr->prot);
		if(alloc_err < 0)
		{
			mem_clear(dst);
			return alloc_err;
		}	
		
		for(uintptr_t ff = sptr->vaddr; ff < sptr->vaddr + sptr->size; ff += pagesize)
		{
			uintptr_t old_frame = m_uspc_get(src->uspc, ff);
			uintptr_t new_frame = m_uspc_get(dst->uspc, ff);
			
			KASSERT(old_frame != 0);
			KASSERT(new_frame != 0);
			
			m_frame_copy(new_frame, old_frame);
		}
	}
	
	return 0;
}
