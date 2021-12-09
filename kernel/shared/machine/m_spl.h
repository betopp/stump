//m_spl.h
//Spinlock functions, per-machine
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_SPL_H
#define M_SPL_H

#include <stdbool.h>

//Type representing a spinlock
typedef volatile int m_spl_t;

//Acquires a spinlock, blocking until it is held.
void m_spl_acq(m_spl_t *spl);

//Attempts to acquire a spinlock, but doesn't block if it is already in use.
//Returns true if the spinlock was acquired, false otherwise.
bool m_spl_try(m_spl_t *spl);

//Releases the given spinlock.
void m_spl_rel(m_spl_t *spl);

#endif //M_SPL_H
