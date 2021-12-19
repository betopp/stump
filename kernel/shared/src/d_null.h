//d_null.h
//Character device: all-consuming black hole
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef D_NULL_H
#define D_NULL_H

#include <sys/types.h>

ssize_t d_null_write(int minor, const void *buf, ssize_t len);
ssize_t d_null_read(int minor, void *buf, ssize_t len);

#endif //D_NULL_H
