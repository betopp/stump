//d_pty.c
//Pseudoterminal device
//Bryan E. Topp <betopp@betopp.com> 2021

#include "d_pty.h"
#include "m_spl.h"
#include "pipe.h"
#include "kassert.h"
#include "process.h"
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <sc.h>
#include <termios.h>

//Information about each pseudoterminal
typedef struct d_pty_s
{
	//Spinlock protecting the pseudoterminal
	m_spl_t spl;
	
	//ID of pipe backing the pseudoterminal
	id_t pipe;
	
	//Process group active on the pseudoterminal
	pid_t pgid;
	
	//How many files are open on the front
	int refs_f;
	
	//How many files are open on the back
	int refs_b;
	
	//Name of the pseudoterminal (not parsed by kernel - stored so usermode can set/get it)
	#define D_PTY_NAME_BUFLEN 32
	char name[D_PTY_NAME_BUFLEN];
	
} d_pty_t;
#define D_PTY_MAX 16
d_pty_t d_pty_table[D_PTY_MAX];


//Handler for ioctls on pseudoterminals once locked
static int d_pty_ioctl_locked(d_pty_t *pptr, bool front, int operation, void *buf, ssize_t len)
{
	
	if(operation == _SC_IOCTL_GETPGRP)
	{
		return pptr->pgid;
	}
	
	if(operation == _SC_IOCTL_SETPGRP)
	{
		pid_t pgid = 0;
		
		if(len != sizeof(pgid))
			return -EINVAL;
		
		int copy_err = process_memget(&pgid, buf, sizeof(pgid));
		if(copy_err < 0)
			return copy_err;
		
		pptr->pgid = pgid;
		return 0;
	}
	
	if(operation == _SC_IOCTL_GETNAME)
	{
		ssize_t needed = strnlen(pptr->name, D_PTY_NAME_BUFLEN) + 1;
		if(len < needed)
			return -ERANGE;
		
		int copy_err = process_memput(buf, pptr->name, needed);
		if(copy_err < 0)
			return copy_err;
		
		return 0;
	}
	
	if(operation == _SC_IOCTL_SETNAME)
	{
		if(front)
			return -ENOTTY;
		
		//Make sure they're not trying to fill more bytes than our buffer supports
		if(len > D_PTY_NAME_BUFLEN)
			return -ERANGE;
		
		char kbuf[D_PTY_NAME_BUFLEN] = {0};
		int copy_err = process_memget(kbuf, buf, len);
		if(copy_err < 0)
			return copy_err;
		
		//Make sure that the buffer, as filled, would be NUL-terminated.
		if(kbuf[sizeof(kbuf)-1] != '\0')
			return -ERANGE;
		
		//Name looks OK, put it into place.
		KASSERT(sizeof(pptr->name) == sizeof(kbuf));
		memcpy(pptr->name, kbuf, sizeof(pptr->name));
		
		return 0;
	}
	
	if(operation == _SC_IOCTL_ISATTY)
	{
		return 1;
	}
		
	return -ENOTTY;
}

//Tries to acquire the lock for the given pseudoterminal.
//Optionally, performs checks for readiness from the front-side (i.e. has somebody created it from the back-side).
static d_pty_t *d_pty_lock(int minor, bool checkready)
{
	if(minor < 0 || minor >= D_PTY_MAX)
	{
		//Bad device number
		return NULL;
	}
	
	d_pty_t *tptr = &(d_pty_table[minor]);
	m_spl_acq(&(tptr->spl));
	
	if(checkready)
	{
		if(tptr->pipe == 0)
		{
			m_spl_rel(&(tptr->spl));
			return NULL;
		}
	}
	
	return tptr; //Still locked
}

//Releases lock on the given pseudoterminal. Frees resources if nobody holds it.
static void d_pty_unlock(d_pty_t *tptr)
{
	KASSERT(tptr->refs_f >= 0);
	KASSERT(tptr->refs_b >= 0);
	
	//Unlocking with no further files open, cleans up resources of the pseudoterminal
	if(tptr->refs_f == 0 && tptr->refs_b == 0)
	{
		if(tptr->pipe > 0)
		{
			pipe_t *pipe = pipe_lockid(tptr->pipe);
			KASSERT(pipe != NULL);
			
			pipe->dirs[PIPE_DIR_FORWARD].refs_r = 0;
			pipe->dirs[PIPE_DIR_FORWARD].refs_w = 0;
			
			pipe->dirs[PIPE_DIR_REVERSE].refs_r = 0;
			pipe->dirs[PIPE_DIR_REVERSE].refs_w = 0;
			
			pipe_delete(pipe);
			pipe = NULL;
			tptr->pipe = 0;	
		}
		
		memset(tptr->name, 0, sizeof(tptr->name));
		tptr->pgid = 0;
	}
	
	m_spl_rel(&(tptr->spl));
}

int d_pty_f_open (int minor)
{
	d_pty_t *pptr = d_pty_lock(minor, false);
	if(pptr == NULL)
		return -ENXIO;
	
	pptr->refs_f++;
	d_pty_unlock(pptr);
	return 0;
}

void d_pty_f_close(int minor)
{
	d_pty_t *pptr = d_pty_lock(minor, false);
	KASSERT(pptr != NULL);
	pptr->refs_f--;
	d_pty_unlock(pptr);
}

ssize_t d_pty_f_read (int minor, void *buf, ssize_t nbytes)
{
	d_pty_t *pptr = d_pty_lock(minor, true);
	if(pptr == NULL)
		return -ENXIO;
	
	pipe_t *pipe = pipe_lockid(pptr->pipe);
	KASSERT(pipe != NULL); //Checked when locking
	
	ssize_t retval = pipe_read(pipe, PIPE_DIR_FORWARD, buf, nbytes);
	
	pipe_unlock(pipe);
	d_pty_unlock(pptr);
	return retval;
}

ssize_t d_pty_f_write(int minor, const void *buf, ssize_t nbytes)
{
	d_pty_t *pptr = d_pty_lock(minor, true);
	if(pptr == NULL)
		return -ENXIO;
	
	pipe_t *pipe = pipe_lockid(pptr->pipe);
	KASSERT(pipe != NULL); //Checked when locking
	
	ssize_t retval = pipe_write(pipe, PIPE_DIR_REVERSE, buf, nbytes);
	
	pipe_unlock(pipe);
	d_pty_unlock(pptr);
	return retval;
}

int d_pty_f_ioctl(int minor, int operation, void *buf, ssize_t len)
{
	d_pty_t *pptr = d_pty_lock(minor, true);
	if(pptr == NULL)
		return -ENXIO;
	
	int retval = d_pty_ioctl_locked(pptr, true, operation, buf, len);
	
	d_pty_unlock(pptr);
	return retval;
}

int d_pty_b_open (int minor)
{
	d_pty_t *pptr = d_pty_lock(minor, false);
	if(pptr == NULL)
		return -ENXIO;
	
	//First one to open the back of a pseudoterminal needs to make the pipe
	if(pptr->refs_b == 0)
	{
		KASSERT(pptr->pipe == 0);
		
		pipe_t *pipe = NULL;
		int pipe_err = pipe_new(&pipe);
		if(pipe_err < 0)
		{
			//Failed to make the pipe backing the pseudoterminal...
			d_pty_unlock(pptr);
			return pipe_err;
		}
		
		//Give the pipe phony readers/writers so, while the pseudoterminal exists, it never returns EPIPE.
		pipe->dirs[PIPE_DIR_FORWARD].refs_r++;
		pipe->dirs[PIPE_DIR_FORWARD].refs_w++;
		
		pipe->dirs[PIPE_DIR_REVERSE].refs_r++;
		pipe->dirs[PIPE_DIR_REVERSE].refs_w++;
		
		pptr->pipe = pipe->id;
		pipe_unlock(pipe);
	}
	
	KASSERT(pptr->pipe > 0);
	pptr->refs_b++;
	
	d_pty_unlock(pptr);
	return 0;
}

void d_pty_b_close(int minor)
{
	d_pty_t *pptr = d_pty_lock(minor, false);
	KASSERT(pptr != NULL);
	pptr->refs_b--;
	d_pty_unlock(pptr);
}

ssize_t d_pty_b_read (int minor, void *buf, ssize_t nbytes)
{
	d_pty_t *pptr = d_pty_lock(minor, true);
	if(pptr == NULL)
		return -ENXIO;
	
	pipe_t *pipe = pipe_lockid(pptr->pipe);
	KASSERT(pipe != NULL); //Checked when locking
	
	ssize_t retval = pipe_read(pipe, PIPE_DIR_REVERSE, buf, nbytes);
	
	pipe_unlock(pipe);
	d_pty_unlock(pptr);
	return retval;
}

ssize_t d_pty_b_write(int minor, const void *buf, ssize_t nbytes)
{
	d_pty_t *pptr = d_pty_lock(minor, true);
	if(pptr == NULL)
		return -ENXIO;
	
	pipe_t *pipe = pipe_lockid(pptr->pipe);
	KASSERT(pipe != NULL); //Checked when locking
	
	ssize_t retval = pipe_write(pipe, PIPE_DIR_FORWARD, buf, nbytes);
	
	pipe_unlock(pipe);
	d_pty_unlock(pptr);
	return retval;
}

int d_pty_b_ioctl(int minor, int operation, void *buf, ssize_t len)
{
	d_pty_t *pptr = d_pty_lock(minor, true);
	if(pptr == NULL)
		return -ENXIO;
	
	int retval = d_pty_ioctl_locked(pptr, false, operation, buf, len);
	
	d_pty_unlock(pptr);
	return retval;
}

