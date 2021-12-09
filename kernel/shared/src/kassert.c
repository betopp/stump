//kassert.c
//Kernel assertion macro
//Bryan E. Topp <betopp@betopp.com> 2021

#include "kassert.h"
#include "m_panic.h"

void kassert_failed(const char *str)
{
	m_panic(str);
}
