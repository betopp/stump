//m_atomic.h
//Atomic operations, per-machine functions
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_ATOMIC_H
#define M_ATOMIC_H

typedef intptr_t m_atomic_t;

//Increments the given atomic variable. Returns its new value.
m_atomic_t m_atomic_increment_and_fetch(volatile m_atomic_t *atomic);

//Decrements the given atomic variable. Returns its new value.
m_atomic_t m_atomic_decrement_and_fetch(volatile m_atomic_t *atomic);

//Attempts to replace the given existing value with the given new value.
//Returns whether the replacement was successful - true if the value was replaced.
//Returns false and does nothing if the existing value is not the latest value of the atomic.
bool m_atomic_cmpxchg(volatile m_atomic_t *atomic, m_atomic_t oldv, m_atomic_t newv);

#endif //M_ATOMIC_H
