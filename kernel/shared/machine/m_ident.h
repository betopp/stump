//m_ident.h
//Machine identity functions, per-machine
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_IDENT_H
#define M_IDENT_H

//Returns a string describing the machine.
const char *m_ident_string(void);

//Returns the ELF machine number for the machine.
int m_ident_elfnum(void);

#endif //M_IDENT_H
