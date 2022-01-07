//m_ident.c
//Machine identity functions on 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

#include "m_ident.h"

const char *m_ident_string(void)
{
	return "arm32";
}

int m_ident_elfnum(void)
{
	return 0x28;
}
