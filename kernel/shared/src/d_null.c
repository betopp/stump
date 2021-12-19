//d_null.c
//Character device: all-consuming black hole
//Bryan E. Topp <betopp@betopp.com> 2021

#include "d_null.h"
#include <errno.h>

ssize_t d_null_write(int minor, const void *buf, ssize_t len)
{
	if(minor != 0)
		return -ENXIO;
	
	(void)buf;
	(void)len;
	return -ENOSPC;
}

ssize_t d_null_read(int minor, void *buf, ssize_t len)
{
	if(minor != 0)
		return -ENXIO;
	
	(void)buf;
	(void)len;
	return 0;
}
