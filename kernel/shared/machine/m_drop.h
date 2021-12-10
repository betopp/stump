//m_drop.h
//Dropping to usermode from kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_DROP_H
#define M_DROP_H

#include <stdint.h>

//Holds user context while in kernel.
typedef struct m_drop_s
{
	uintptr_t words[32]; //eh
} m_drop_t;

//Resets a user context for execution at the given entry point.
void m_drop_reset(m_drop_t *drop, uintptr_t entry);

//Changes the return-value stored in the given user context.
void m_drop_retval(m_drop_t *drop, uintptr_t retval);

//Drops to userspace using the given context.
void m_drop(const m_drop_t *drop);

#endif //M_DROP_H
