//pipe.c
//Pipes in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "pipe.h"
#include "kpage.h"
#include "kassert.h"
#include "thread.h"
#include <string.h>
#include <sc.h>
#include <errno.h>

//All pipes in the system
#define PIPE_MAX 1024
static pipe_t pipe_table[PIPE_MAX];

//Puts the current thread on the list of waiters. Unpauses one to take its place if necessary.
static void pipe_addwaiter(id_t waiters[])
{
	id_t curtid = thread_curtid();
	
	for(int ww = 0; ww < PIPE_WAITING_MAX; ww++)
	{
		if(waiters[ww] == 0 || waiters[ww] == curtid)
		{
			waiters[ww] = curtid;
			return;
		}
	}
	
	//Ugly but technically correct. They'll waste some CPU cycles and re-pause. It'll thrash.
	thread_unpause(waiters[0]);
	waiters[0] = curtid;
	return;
}

//Unpauses all threads on the list of those waiting.
static void pipe_kickwaiters(id_t waiters[])
{
	for(int ww = 0; ww < PIPE_WAITING_MAX; ww++)
	{
		if(waiters[ww])
			thread_unpause(waiters[ww]);
		
		waiters[ww] = 0;
	}
}

//Returns how many more bytes can be read from the given pipe's buffer before it is empty.
static ssize_t pipe_canread(const pipe_t *p, pipe_dir_t dir)
{
	ssize_t r = p->dirs[dir].rptr;
	ssize_t w = p->dirs[dir].wptr;
	if(w < r)
		w += p->dirs[dir].buf_len;
	
	return w - r;
}

//Returns how many bytes can be read in a single shot from the pipe's buffer (without wrapping).
static ssize_t pipe_canread_single(const pipe_t *p, pipe_dir_t dir)
{
	ssize_t to_buf = p->dirs[dir].buf_len - p->dirs[dir].rptr;
	ssize_t to_wptr = pipe_canread(p, dir);
	
	return (to_wptr < to_buf) ? to_wptr : to_buf;
}

//Returns how many more bytes can be written to the given pipe's buffer before it is full.
static ssize_t pipe_canwrite(const pipe_t *p, pipe_dir_t dir)
{
	ssize_t r = p->dirs[dir].rptr;
	ssize_t w = p->dirs[dir].wptr;
	if(w < r)
		w += p->dirs[dir].buf_len;
	
	return (p->dirs[dir].buf_len - 1) + r - w;
}

//Returns how many bytes can be written in a single shot to the pipe's buffer (without wrapping).
static ssize_t pipe_canwrite_single(const pipe_t *p, pipe_dir_t dir)
{
	ssize_t to_buf = p->dirs[dir].buf_len - p->dirs[dir].wptr;
	ssize_t to_rptr = pipe_canwrite(p, dir);
	
	return (to_rptr < to_buf) ? to_rptr : to_buf;
}

int pipe_new(pipe_t **pipe_out)
{
	//Find a spot for the pipe in the pipe table
	pipe_t *pptr = NULL;
	for(int pp = 0; pp < PIPE_MAX; pp++)
	{
		if(m_spl_try(&(pipe_table[pp].spl)))
		{
			if(pipe_table[pp].dirs[PIPE_DIR_FORWARD].buf_len == 0)
			{
				//Found a free spot.
				pptr = &(pipe_table[pp]);
				
				//Make sure the pipe has a valid ID that maps to its location in the table.
				//Advance IDs each time we use a slot.
				pptr->id += PIPE_MAX;
				if( (pptr->id <= 0) || ((pptr->id % PIPE_MAX) != pp) )
					pptr->id = pp;
	
				break;
			}
			
			//Not free, keep looking.
			m_spl_rel(&(pipe_table[pp].spl));
		}
	}
	
	if(pptr == NULL)
	{
		//No free spots
		return -ENFILE;
	}
	
	//Allocate buffers for all pipe dimensions.
	size_t pipebuf = 4096;
	for(int dir = PIPE_DIR_NONE + 1; dir < PIPE_DIR_MAX; dir++)
	{
		//Shouldn't have leftover buffer; clear old structure
		KASSERT(pptr->dirs[dir].buf_ptr == NULL);
		KASSERT(pptr->dirs[dir].buf_len == 0);
		memset(&(pptr->dirs[dir]), 0, sizeof(pptr->dirs[dir]));
		
		//Allocate buffer for this direction
		pptr->dirs[dir].buf_ptr = kpage_alloc(pipebuf);
		if(pptr->dirs[dir].buf_ptr == NULL)
		{
			//No room for buffer for this pipe direction.
			//Free buffers for other directions that we did allocate.
			while(dir > PIPE_DIR_NONE + 1)
			{
				dir--;
				kpage_free(pptr->dirs[dir].buf_ptr, pptr->dirs[dir].buf_len);
				pptr->dirs[dir].buf_ptr = NULL;
				pptr->dirs[dir].buf_len = 0;
			}
			
			//Not enough memory to construct the pipe.
			m_spl_rel(&(pptr->spl));
			return -ENOMEM;
		}
		pptr->dirs[dir].buf_len = pipebuf;
	}
	
	KASSERT(pptr->id > 0);
	*pipe_out = pptr;
	return 0; //Still locked
}

void pipe_delete(pipe_t *pipe)
{
	for(int dd = PIPE_DIR_NONE + 1; dd < PIPE_DIR_MAX; dd++)
	{
		KASSERT(pipe->dirs[dd].refs_r == 0);
		KASSERT(pipe->dirs[dd].refs_w == 0);
		
		kpage_free(pipe->dirs[dd].buf_ptr, pipe->dirs[dd].buf_len);
		memset(&(pipe->dirs[dd]), 0, sizeof(pipe->dirs[dd]));
	}
	m_spl_rel(&(pipe->spl));
}

pipe_t *pipe_lockid(int id)
{
	if(id <= 0)
		return NULL;
	
	pipe_t *pptr = &(pipe_table[id % PIPE_MAX]);
	m_spl_acq(&(pptr->spl));
	if((pptr->dirs[PIPE_DIR_FORWARD].buf_len == 0) || pptr->id != id)
	{
		//No/wrong pipe here
		m_spl_rel(&(pptr->spl));
		return NULL;
	}
	
	//Still locked
	return pptr;
}

void pipe_unlock(pipe_t *pptr)
{
	m_spl_rel(&(pptr->spl));
}

ssize_t pipe_write(pipe_t *pptr, pipe_dir_t dir, const void *buf, ssize_t nbytes)
{		
	//POSIX requires that writes of 512 bytes or less can complete atomically.
	//So just don't allow writing to pipes with less than 512 bytes in their buffer.
	if(pipe_canwrite(pptr, dir) < 512)
	{
		if(pptr->dirs[dir].refs_r)
		{
			//No room in the pipe, but someone has it open to read and could drain it.
			pipe_addwaiter(pptr->dirs[dir].waiting_to_w);
			return -EAGAIN;
		}
		
		return -EPIPE; //No room in the pipe and no one draining it
	}
		
	//Write as much as we can
	const char *buf_bytes = (const char*)buf;
	ssize_t written = 0;
	while(1)
	{
		ssize_t copylen = pipe_canwrite_single(pptr, dir);
		if(copylen > nbytes)
			copylen = nbytes;
		
		if(copylen == 0)
			break;
		
		memcpy(pptr->dirs[dir].buf_ptr + pptr->dirs[dir].wptr, buf_bytes, copylen);
		pptr->dirs[dir].wptr = (pptr->dirs[dir].wptr + copylen) % (pptr->dirs[dir].buf_len);
		
		buf_bytes += copylen;
		written += copylen;
		nbytes -= copylen;
	}
	
	//Unpause anyone waiting to read from this pipe, if we just wrote
	pipe_kickwaiters(pptr->dirs[dir].waiting_to_r);
	
	return written;
}

ssize_t pipe_read(pipe_t *pptr, pipe_dir_t dir, void *buf, ssize_t nbytes)
{	
	if(pipe_canread(pptr, dir) < 1)
	{
		if(pptr->dirs[dir].refs_w)
		{
			//Pipe empty, but has writers who could fill it.
			pipe_addwaiter(pptr->dirs[dir].waiting_to_r);
			return -EAGAIN;
		}
		
		return -EPIPE; //Pipe empty and no one filling it
	}
	
	//Read as much as we can
	char *buf_bytes = (char*)buf;
	ssize_t nread = 0;
	while(1)
	{
		ssize_t copylen = pipe_canread_single(pptr, dir);
		if(copylen > nbytes)
			copylen = nbytes;
		
		if(copylen == 0)
			break;
		
		memcpy(buf_bytes, pptr->dirs[dir].buf_ptr + pptr->dirs[dir].rptr, copylen);
		pptr->dirs[dir].rptr = (pptr->dirs[dir].rptr + copylen) % (pptr->dirs[dir].buf_len);
		
		buf_bytes += copylen;
		nread += copylen;
		nbytes -= copylen;
	}
	
	//Unpause anyone waiting to write to this pipe, if we just read
	pipe_kickwaiters(pptr->dirs[dir].waiting_to_w);
	
	return nread;
}

int pipe_ioctl(pipe_t *pptr, pipe_dir_t dir, int operation, void *buf, ssize_t len)
{
	(void)buf;
	(void)len;
	(void)dir;
	
	switch(operation)
	{
		case _SC_IOCTL_ISATTY:
		{
			//If we've got references both forward and reverse, this "is a TTY".
			bool fwd = pptr->dirs[PIPE_DIR_FORWARD].refs_r || pptr->dirs[PIPE_DIR_FORWARD].refs_w;
			bool rev = pptr->dirs[PIPE_DIR_REVERSE].refs_r || pptr->dirs[PIPE_DIR_REVERSE].refs_w;
			if(fwd && rev)
				return 1;
			else
				return 0;
		}
		default:
			return -ENOTTY;
	}
}
