//m_atomic.h
//Atomic operations, per-machine functions
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_ATOMIC_H
#define M_ATOMIC_H

typedef int64_t m_atomic_t;

//Increments the given atomic variable. Returns its new value.
m_atomic_t m_atomic_increment_and_fetch(volatile m_atomic_t *atomic);

//Decrements the given atomic variable. Returns its new value.
m_atomic_t m_atomic_decrement_and_fetch(volatile m_atomic_t *atomic);

#endif //M_ATOMIC_H
