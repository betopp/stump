//d_pty.h
//Pseudoterminal device
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef D_PTY_H
#define D_PTY_H

#include <sys/types.h>

//We use "front" to refer to the normal /dev/pty or whatever that a user will access.
//The "back" of the device is opened by the terminal emulator to service the PTY.

int     d_pty_f_open (int minor);
void    d_pty_f_close(int minor);
ssize_t d_pty_f_read (int minor, void *buf, ssize_t nbytes);
ssize_t d_pty_f_write(int minor, const void *buf, ssize_t nbytes);
int     d_pty_f_ioctl(int minor, int operation, void *buf, ssize_t len);

int     d_pty_b_open (int minor);
void    d_pty_b_close(int minor);
ssize_t d_pty_b_read (int minor, void *buf, ssize_t nbytes);
ssize_t d_pty_b_write(int minor, const void *buf, ssize_t nbytes);
int     d_pty_b_ioctl(int minor, int operation, void *buf, ssize_t len);

#endif //D_PTY_H
