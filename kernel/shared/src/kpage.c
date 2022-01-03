//kpage.c
//Kernel page allocator
//Bryan E. Topp <betopp@betopp.com> 2021

#include "kassert.h"
#include "kpage.h"
#include "m_frame.h"
#include "m_spl.h"
#include "m_kspc.h"
#include <string.h>

//Range of usable kernel addresses
static uintptr_t kpage_start;
static uintptr_t kpage_end;

//Next address we try - bump allocator!
static uintptr_t kpage_next;

//Spinlock protecting kernel page allocator
static m_spl_t kpage_spl;

//Finds a free region of the given number of pages.
static uintptr_t kpage_findfree(size_t pages_needed)
{
	//Okay I wanted a bump allocator but I don't have a good way of freeing unneeded kernel pagetables.
	//Probably just as wasteful either way - scanning for totally-0 PTs or starting over each time here.
	kpage_next = 0;
	
	//Do a stupid linear search through our kernel-space until we find that many free pages.
	size_t pagesize = m_frame_size();
	size_t maxattempts = (kpage_end - kpage_start) / pagesize;
	uintptr_t found_start = 0;
	size_t found_pages = 0;
	for(size_t attempt = 0; attempt < maxattempts; attempt++)
	{
		//Search the next page, in-order.
		//Wrap around our search point, if we hit the end of the address space
		kpage_next += pagesize;
		if(kpage_next < kpage_start || kpage_next >= kpage_end)
		{
			kpage_next = kpage_start;
			
			//Can't span an allocation across the wraparound
			found_start = 0;
			found_pages = 0;
		}
		
		if(m_kspc_get(kpage_next) != 0)
		{
			//This page isn't free. Can't use it.
			found_start = 0;
			found_pages = 0;
		}
		else
		{
			//This page is free. We can use this.
			if(found_pages == 0)
				found_start = kpage_next; //First usable page after unusable pages
			
			found_pages++;
			if(found_pages >= pages_needed)
				break;
		}
	}
	
	if(found_pages < pages_needed)
	{
		//Didn't find enough free virtual kernel-space for this allocation (!?!?!?!).
		return 0;
	}	
	
	return found_start;
}

void kpage_init(void)
{
	//Figure out range of usable addresses
	m_kspc_range(&kpage_start, &kpage_end);
}

void *kpage_alloc(size_t nbytes)
{
	m_spl_acq(&kpage_spl);
	
	//Figure out how many pages we'll need
	size_t pagesize = m_frame_size();
	size_t pages_needed = (nbytes + pagesize - 1) / pagesize;
	
	//We'll look for 2 more pages than that - to leave guard pages around each allocation.
	pages_needed += 2;
	
	//Do a stupid linear search through our kernel-space until we find that many free pages.
	uintptr_t found_start = kpage_findfree(pages_needed);
	if(found_start == 0)
	{
		m_spl_rel(&kpage_spl);
		return NULL;
	}
	
	//Drop the guard pages from what we'll allocate
	pages_needed -= 2;
	found_start += pagesize;
	
	//Try to allocate frames and back this region
	for(uintptr_t pp = found_start; pp < (found_start + (pagesize * pages_needed)); pp += pagesize)
	{
		uintptr_t newframe = m_frame_alloc();
		if(newframe != 0)
		{
			//Got a frame. Zero it and put it in kernel-space here.
			bool mapped = m_kspc_set(pp, newframe);
			if(mapped)
			{
				//Success. Keep going.
				continue;
			}
		}
		
		//Failed to allocate or map the next frame.
		
		//Put the frame back if we allocated one
		if(newframe != 0)
		{
			m_frame_free(newframe);
			newframe = 0;
		}
		
		//Unwind any allocation we partially completed.
		while(pp > found_start)
		{
			pp -= pagesize;
			uintptr_t unwind = m_kspc_get(pp);
			KASSERT(unwind != 0); //We just mapped this page, it should still be here
			m_kspc_set(pp, 0);
			m_frame_free(unwind);
		}
		
		//Didn't have enough memory to complete the allocation
		m_spl_rel(&kpage_spl);
		return NULL;
	}
	
	//Success - new pages are now ready.
	m_spl_rel(&kpage_spl);
	void *retval = (void*)found_start;
	return retval;
}

void kpage_free(void *ptr, size_t nbytes)
{
	m_spl_acq(&kpage_spl);
	
	//Find the number of pages we'll be freeing - same as in kpage_alloc.
	size_t pagesize = m_frame_size();
	size_t npages = (nbytes + pagesize - 1) / pagesize;
	
	//Unmap and free that many frames
	for(uintptr_t pp = (uintptr_t)ptr; pp < (uintptr_t)ptr + (npages * pagesize); pp += pagesize)
	{
		uintptr_t frame = m_kspc_get(pp);
		KASSERT(frame != 0);
		m_kspc_set(pp, 0);
		m_frame_free(frame);
	}
	
	m_spl_rel(&kpage_spl);
}

void *kpage_physadd(uintptr_t paddr, size_t nbytes)
{
	m_spl_acq(&kpage_spl);
	
	size_t pagesize = m_frame_size();
	size_t pages_needed = (nbytes + pagesize - 1) / pagesize;
	pages_needed += 2;
	
	uintptr_t found_start = kpage_findfree(pages_needed);
	if(found_start == 0)
	{
		m_spl_rel(&kpage_spl);
		return NULL;
	}
	
	pages_needed -= 2;
	found_start += pagesize;
	
	//Try to allocate frames and back this region
	for(uintptr_t pp = found_start; pp < (found_start + (pagesize * pages_needed)); pp += pagesize)
	{
		//Try to map the appropriate physical frame here
		bool mapped = m_kspc_set(pp, paddr + (pp - found_start));
		if(mapped)
		{
			//Success. Keep going.
			continue;
		}
		
		//Failed to map the next frame.
		//Unwind any allocation we partially completed.
		while(pp > found_start)
		{
			pp -= pagesize;
			uintptr_t unwind = m_kspc_get(pp);
			KASSERT(unwind != 0); //We just mapped this page, it should still be here
			m_kspc_set(pp, 0);
		}
		
		//Didn't have enough room for paging structures (?) to complete the allocation
		m_spl_rel(&kpage_spl);
		return NULL;
	}
	
	//Success - new pages are now ready.
	m_spl_rel(&kpage_spl);
	void *retval = (void*)found_start;
	return retval;	
}

void kpage_physdel(void *ptr, size_t nbytes)
{
	m_spl_acq(&kpage_spl);
	
	//Find the number of pages we'll be freeing - same as in kpage_physadd.
	size_t pagesize = m_frame_size();
	size_t npages = (nbytes + pagesize - 1) / pagesize;
	
	//Unmap that many frames - but don't free them
	for(uintptr_t pp = (uintptr_t)ptr; pp < (uintptr_t)ptr + (npages * pagesize); pp++)
	{
		uintptr_t frame = m_kspc_get(pp);
		KASSERT(frame != 0);
		m_kspc_set(pp, 0);
	}
	
	m_spl_rel(&kpage_spl);	
}
