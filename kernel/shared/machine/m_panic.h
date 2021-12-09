//m_panic.h
//Fatal errors, per-machine handling
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_PANIC_H
#define M_PANIC_H

//Stops the machine and, if possible, outputs the given error string.
void m_panic(const char *str) __attribute__((noreturn));

#endif //M_PANIC_H

