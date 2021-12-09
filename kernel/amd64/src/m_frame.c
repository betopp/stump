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

void m_frame_init(void)
{
	//Memory map from multiboot bootloader, which we set aside earlier
	extern uint32_t multiboot_mmap_size;
	extern uint8_t multiboot_mmap_storage[];
	
	uint64_t pagesize = m_frame_size();
	
	//Run through memory map looking for usable frames
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
			for(uint64_t ii = start_idx; ii < end_idx; ii++)
			{
				m_frame_free(ii * pagesize);
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
		//No frames on free-list.
		m_spl_rel(&m_frame_spl);
		return 0;
	}
	
	//We'll return the old head of the free-list.
	uintptr_t retval = m_frame_head;
	
	//The page to which the old head refers, is now the head.
	m_frame_head = pspace_read(m_frame_head);
	
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
