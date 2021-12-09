//m_kspc.h
//Kernel space management, per-machine
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_KSPC_H
#define M_KSPC_H

#include <stdint.h>
#include <stdbool.h>

//Outputs the range of addresses usable for dynamic kernel-space mapping.
//Start is inclusive, end is exclusive.
void m_kspc_range(uintptr_t *start_out, uintptr_t *end_out);

//Changes the mapping for the given kernel-space page.
//Passing 0 for paddr will unmap the page.
//Returns true on success; returns false on failure (probably: out of room for pagetables).
bool m_kspc_set(uintptr_t vaddr, uintptr_t paddr);

//Returns the frame backing the given kernel-space page.
//Returns 0 if the page is not mapped.
uintptr_t m_kspc_get(uintptr_t vaddr);

#endif //M_KSPC_H
