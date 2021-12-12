//mem.h
//User memory space tracking
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef MEM_H
#define MEM_H

#include <stddef.h>
#include <stdint.h>
#include "m_uspc.h"

//One segment of memory allocated in a memory space
typedef struct mem_seg_s
{
	uintptr_t vaddr;
	size_t size;
	int prot;
	
} mem_seg_t;

//Memory space
typedef struct mem_s
{
	//Kernel-side tracking of which segments are allocated
	#define MEM_SEG_MAX 32
	mem_seg_t segs[MEM_SEG_MAX];
	
	//Machine-specific paging structures
	m_uspc_t uspc;
} mem_t;

//Frees all memory in a memory space. The memory space can then be zeroed.
void mem_clear(mem_t *mem);

//Allocates new memory and adds it to a memory space.
int mem_add(mem_t *mem, uintptr_t vaddr, size_t size, int prot);

#endif //MEM_H
