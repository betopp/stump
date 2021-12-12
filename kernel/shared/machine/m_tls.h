//m_tls.h
//TLS pointer storage, per-machine
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_TLS_H
#define M_TLS_H

//Sets the current CPU's TLS pointer.
void m_tls_set(void *ptr);

//Returns the current CPU's TLS pointer.
void *m_tls_get(void);


#endif //M_TLS_H
