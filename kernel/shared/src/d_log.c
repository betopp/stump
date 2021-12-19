//d_log.c
//Character device: kernel log
//Bryan E. Topp <betopp@betopp.com> 2021

#include "d_log.h"
#include <errno.h>

ssize_t d_log_write(int minor, const void *buf, ssize_t len)
{
	if(minor != 0)
		return -ENXIO;
	
	//Todo - spew this out a serial terminal or buffer it up or something.
	(void)buf;
	(void)len;
	return len;
}
