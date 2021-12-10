//m_uspc.h
//Userspace management, per-machine functions
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_USPC_H
#define M_USPC_H


#include <stdint.h>
#include <stdbool.h>

//Identifies userspace data that the machine can use.
//Todo - does this need to be different for each machine?
typedef uintptr_t m_uspc_t;

//Outputs the range of addresses usable for user-space on this machine.
void m_uspc_range(uintptr_t *start_out, uintptr_t *end_out);

//Makes a new userspace. Returns an identifier for it.
m_uspc_t m_uspc_new(void);

//Frees all data about a userspace (does not free the frames to which it refers).
void m_uspc_delete(m_uspc_t uspc);

//Access allowed to userspace pages
#define M_USPC_PROT_R 4
#define M_USPC_PROT_W 2
#define M_USPC_PROT_X 1

//Changes the mapping of a page in userspace.
//Returns true if the mapping was made; false otherwise (probably: out of physical RAM).
bool m_uspc_set(m_uspc_t uspc, uintptr_t vaddr, uintptr_t paddr, int prot);

//Returns the frame backing the given page in userspace.
uintptr_t m_uspc_get(m_uspc_t uspc, uintptr_t vaddr);


//Returns the currently-active userspace.
m_uspc_t m_uspc_current();

//Changes the currently-active userspace.
void m_uspc_activate(m_uspc_t uspc);


#endif //M_USPC_H
