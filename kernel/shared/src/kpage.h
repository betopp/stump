//kpage.h
//Kernel page allocator
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef KPAGE_H
#define KPAGE_H

#include <stddef.h>
#include <stdint.h>

//Initializes kernel page allocator
void kpage_init(void);

//Allocates kernel pages to cover the given number of contiguous bytes.
void *kpage_alloc(size_t nbytes);

//Frees kernel pages previously allocated with kpage_alloc.
void kpage_free(void *ptr, size_t nbytes);

//Maps a range of physical addresses.
void *kpage_physadd(uintptr_t paddr, size_t nbytes);

//Unmaps a range of physical addresses previously mapped with kpage_physadd.
void kpage_physdel(void *ptr, size_t nbytes);

#endif //KPAGE_H
