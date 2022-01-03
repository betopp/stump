//num.c
//Numerical functions in libc
//Bryan E. Topp <betopp@betopp.com> 2021

#include <stdlib.h>

int abs(int j)
{
	return (j<0)?(-j):(j);
}
