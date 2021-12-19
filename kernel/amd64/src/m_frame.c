//m_frame.c
//Physical memory allocator on AMD64
//Bryan E. Topp <betopp@betopp.com> 2021

#include "m_frame.h"
#include "m_spl.h"
#include "m_panic.h"
#include "pspace.h"

//Spinlock protecting frame allocator
static m_spl_t m_frame_spl;

//Head of free-list
static uintptr_t m_frame_head;

//Number of entries on free-list
static size_t m_frame_count;

//Ranges of memory given by bootloader, not yet put on free-list.
#define M_FRAME_RANGE_MAX 16
static size_t m_frame_range_sizes[M_FRAME_RANGE_MAX];
static uintptr_t m_frame_range_addrs[M_FRAME_RANGE_MAX];

void m_frame_init(void)
{
	//Memory map from multiboot bootloader, which we set aside earlier
	extern uint32_t multiboot_mmap_size;
	extern uint8_t multiboot_mmap_storage[];
	
	uint64_t pagesize = m_frame_size();
	
	//Run through memory map looking for usable frames
	int ranges_used = 0;
	uint8_t *entry = multiboot_mmap_storage;
	while(entry < multiboot_mmap_storage + multiboot_mmap_size)
	{
		uint64_t base   = *((uint64_t*)(entry+4));
		uint64_t length = *((uint64_t*)(entry+12));
		uint32_t type   = *((uint32_t*)(entry+20));
		if(type == 1)
		{
			//Usable memory. Any pages entirely within it, can be added to the free list.
			uint64_t start = base;
			uint64_t end = base + length;
			
			//But don't permit memory before the kernel-end to be used
			extern uint8_t _MULTIBOOT_BSS_END_ADDR[];
			uint64_t min = ((uintptr_t)_MULTIBOOT_BSS_END_ADDR + pagesize - 1) / pagesize;
			
			uint64_t start_idx = (start + pagesize - 1) / pagesize;
			if(start_idx < min)
				start_idx = min;
			
			uint64_t end_idx = end / pagesize;
			
			/*for(uint64_t ii = start_idx; ii < end_idx; ii++)
			{
				m_frame_free(ii * pagesize);
			}*/
			
			if(end_idx > start_idx)
			{
				if(ranges_used >= M_FRAME_RANGE_MAX)
					m_panic("m_frame_init mmap crazy");
				
				m_frame_range_addrs[ranges_used] = start_idx * pagesize;
				m_frame_range_sizes[ranges_used] = (end_idx - start_idx) * pagesize;
				ranges_used++;
			}
		}	
		
		entry += *(uint32_t*)(entry) + 4;
	}
}

size_t m_frame_size(void)
{
	//Normal i386 4K frames
	return 4096;
}

uintptr_t m_frame_alloc(void)
{
	m_spl_acq(&m_frame_spl);
	
	//Head of list, and count of items, should agree on whether we're empty.
	if( (m_frame_head == 0) ^ (m_frame_count == 0) )
		m_panic("m_frame_alloc corrupt freelist");
	
	if(m_frame_head == 0)
	{
		//No frames on free-list. See if we have any frames from the bootloader we have never used.
		for(int rr = 0; rr < M_FRAME_RANGE_MAX; rr++)
		{
			if(m_frame_range_sizes[rr] > 0)
			{
				//Hack off the last frame of the range and return it
				m_frame_range_sizes[rr] -= m_frame_size();
				uintptr_t retval = m_frame_range_addrs[rr] + m_frame_range_sizes[rr];
				m_spl_rel(&m_frame_spl);
				return retval;
			}
		}
		
		//No frames on the free-list and no ranges from the bootloader yet unused.
		//We're out of memory.
		m_spl_rel(&m_frame_spl);
		return 0;
	}
	
	//We'll return the old head of the free-list.
	uintptr_t retval = m_frame_head;
	
	//The page to which the old head refers, is now the head.
	m_frame_head = pspace_read(m_frame_head);
	
	m_frame_count--;
	
	m_spl_rel(&m_frame_spl);
	return retval;
}

void m_frame_free(uintptr_t frame)
{
	m_spl_acq(&m_frame_spl);
	
	//Write the existing head into the frame being freed
	pspace_write(frame, m_frame_head);
	
	//The frame being freed is now the head
	m_frame_head = frame;
	
	//Keep track
	m_frame_count++;
	
	m_spl_rel(&m_frame_spl);
}

void m_frame_copy(uintptr_t newframe, uintptr_t oldframe)
{
	for(uintptr_t ff = 0; ff < 4096; ff += 8)
	{
		pspace_write(newframe + ff, pspace_read(oldframe + ff));
	}
}
