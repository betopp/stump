//m_panic.c
//Fatal errors on AMD64
//Bryan E. Topp <betopp@betopp.com> 2021

#include "m_panic.h"

void __attribute__((noreturn)) m_panic(const char *str)
{
	(void)str;
	while(1) { }
}


