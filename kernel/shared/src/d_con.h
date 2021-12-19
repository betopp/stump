//d_con.h
//Character device: computer console
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef D_CON_H
#define D_CON_H

#include <sys/types.h>

int d_con_open(int minor);
void d_con_close(int minor);
int d_con_ioctl(int minor, int operation, void *buf, ssize_t len);

#endif //D_CON_H
