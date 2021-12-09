//rlimit.c
//rlimit calls in libc
//Bryan E. Topp <betopp@betopp.com> 2021

#include <sys/resource.h>
#include <errno.h>
#include <sc.h>

int setrlimit(int resource, const struct rlimit *rlp)
{
	_sc_rlimit_t l = {0};
	l.cur = rlp->rlim_cur;
	l.max = rlp->rlim_max;
	int err = _sc_setrlimit(resource, &l, sizeof(l));
	if(err < 0)
	{
		errno = -err;
		return -1;
	}
	return 0;
}

int getrlimit(int resource, struct rlimit *rlp)
{
	_sc_rlimit_t l = {0};
	int err = _sc_getrlimit(resource, &l, sizeof(l));
	if(err < 0)
	{
		errno = -err;
		return -1;
	}
	
	rlp->rlim_cur = l.cur;
	rlp->rlim_max = l.max;
	return 0;
}
