//fork.c
//Process creation in libc
//Bryan E. Topp <betopp@betopp.com> 2021

#include <unistd.h>
#include <errno.h>
#include <sc.h>

pid_t fork(void)
{
	pid_t result = _sc_fork();
	if(result < 0)
	{
		errno = -result;
		return -1;
	}
	return result;
}
