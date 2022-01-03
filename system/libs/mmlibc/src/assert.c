//assert.c
//Assertion failure in libc
//Bryan E. Topp <betopp@betopp.com> 2021

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

char assert_buffer[256];

void _assertion_failed(const char *cond, const char *file, const char *func, int line)
{
	snprintf(assert_buffer, sizeof(assert_buffer)-1, "Assertion failed: %s in %s at %s:%d.\n", cond, func, file, line);
	fprintf(stderr, assert_buffer);
	abort();
}
