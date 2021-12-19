//d_nxio.c
//Character device: bad major number
//Bryan E. Topp <betopp@betopp.com> 2021

#include "d_nxio.h"
#include <errno.h>

int d_nxio_open(int minor)
{
	(void)minor;
	return -ENXIO;
}

void d_nxio_close(int minor)
{
	(void)minor;
}

ssize_t d_nxio_read(int minor, void *buf, ssize_t nbytes)
{
	(void)minor;
	(void)buf;
	(void)nbytes;
	return -ENXIO;
}

ssize_t d_nxio_write(int minor, const void *buf, ssize_t nbytes)
{
	(void)minor;
	(void)buf;
	(void)nbytes;
	return -ENXIO;
}

int d_nxio_ioctl(int minor, int operation, void *buf, ssize_t len)
{
	(void)minor;
	(void)operation;
	(void)buf;
	(void)len;
	return -ENXIO;
}

