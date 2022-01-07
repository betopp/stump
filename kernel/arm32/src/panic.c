//panic.c
//Panic implementation on 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

#include "m_panic.h"

void m_panic(const char *str)
{
	(void)str;
	while(1) { }
}
