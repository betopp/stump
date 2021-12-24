//sc_mem.h
//System call library - memory space functions
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef _SC_MEM_H
#define _SC_MEM_H

#include <sys/types.h>
#include <stdint.h>

//Returns an address where the calling process has empty address space of the given size.
intptr_t _sc_mem_avail(intptr_t around, ssize_t size);

//Adds new, private, zeroed memory to the calling process's memory space.
int _sc_mem_anon(uintptr_t addr, ssize_t size, int access);

//Removes memory from the calling process's memory space.
int _sc_mem_free(uintptr_t addr, ssize_t size);

#endif //_SC_MEM_H
