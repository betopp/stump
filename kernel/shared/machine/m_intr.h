//m_intr.h
//Interrupt control, per-machine functions
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_INTR_H
#define M_INTR_H

#include <stdbool.h>

//Enables or disables interrupt handling
void m_intr_ei(bool enable);

//Atomically halts with interrupts enabled, catching immediately any pending interrupts.
void m_intr_halt(void);

#endif //M_INTR_H
