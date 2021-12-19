//d_nxio.h
//Character device: bad major number
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef D_NXIO_H
#define D_NXIO_H

#include <sys/types.h>

int d_nxio_open(int minor);
void d_nxio_close(int minor);
ssize_t d_nxio_read(int minor, void *buf, ssize_t nbytes);
ssize_t d_nxio_write(int minor, const void *buf, ssize_t nbytes);
int d_nxio_ioctl(int minor, int operation, void *buf, ssize_t len);

#endif //D_NXIO_H

