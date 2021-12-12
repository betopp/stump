//m_drop.h
//Dropping to usermode from kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_DROP_H
#define M_DROP_H

#include <stdint.h>

//Reasons that the machine may come back to kernel mode.
typedef enum m_drop_result_e
{
	M_DROP_RESULT_NONE = 0, //None/invalid
	
	M_DROP_RESULT_SYSCALL, //Came back to kernel for system-call
	M_DROP_RESULT_EXCEPTION, //User-mode caused exception
	
	M_DROP_RESULT_MAX,
	
} m_drop_result_t;

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
void m_drop(const m_drop_t *drop) __attribute__((noreturn));

#endif //M_DROP_H
