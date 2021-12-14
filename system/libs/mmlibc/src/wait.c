//wait.c
//Child status functions in libc
//Bryan E. Topp <betopp@betopp.com> 2021

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <sc.h>

pid_t wait(int *status)
{
	return waitpid(-1, status, 0);
}

pid_t waitpid(pid_t wpid, int *status, int options)
{
	idtype_t wait_idtype = P_ALL;
	int64_t wait_id = 0;
	if(wpid == -1)
	{
		wait_idtype = P_ALL;
		wait_id = 0;
	}
	else if(wpid == 0)
	{
		wait_idtype = P_PGID;
		wait_id = getpgrp();
	}
	else if(wpid > 0)
	{
		wait_idtype = P_PID;
		wait_id = wpid;
	}
	else if(wpid < -1)
	{
		wait_idtype = P_PGID;
		wait_id = -wpid;
	}
	
	options |= WEXITED;
	
	//_sc_wait in our kernel doesn't actually support waiting.
	//It always behaves like WNOHANG in a normal kernel.
	//So if we want to really wait here, check for status in a loop and _sc_pause inbetween.
	while(1)
	{
		_sc_wait_t w = {0};
		ssize_t result = _sc_wait(wait_idtype, wait_id, options, &w, sizeof(w));
		if(result > 0)
		{
			*status = w.status;
			return w.pid;
		}
		else if(result < 0)
		{
			errno = -result;
			return -1;
		}
		else if(options & WNOHANG)
		{
			return 0;
		}
		
		_sc_pause();
	}
}

int nice(int incr)
{	
	int old = _sc_priority(P_PID, -1, -1);
	if(old < 0)
	{
		errno = -old;
		return -1;
	}
	
	int newval = old + incr;
	if(newval < 0)
		newval = 0;
	if(newval > 99)
		newval = 99;
	
	int result = _sc_priority(P_PID, -1, newval);
	if(result < 0)
	{
		errno = -result;
		return -1;
	}
	
	return 0;
}
