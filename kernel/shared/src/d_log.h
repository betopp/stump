//d_log.h
//Character device: kernel log
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef D_LOG_H
#define D_LOG_H

#include <sys/types.h>

ssize_t d_log_write(int minor, const void *buf, ssize_t len);

#endif //D_LOG_H