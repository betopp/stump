//pipe.c
//Pipes in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "pipe.h"
#include "kpage.h"
#include "kassert.h"
#include <string.h>
#include <errno.h>

//All pipes in the system
#define PIPE_MAX 1024
static pipe_t pipe_table[PIPE_MAX];

//Checks references to the given pipe. If unreferenced, cleans it up.
static void pipe_checkrefs(pipe_t *pipe)
{
	KASSERT(pipe->refs_ino >= 0);	
	KASSERT(pipe->refs_file >= 0);
	KASSERT(pipe->refs_file_r >= 0);
	KASSERT(pipe->refs_file_w >= 0);
	
	if(pipe->refs_ino > 0)
		return;
	
	if(pipe->refs_file > 0)
		return;
	
	//refs_file == 0 implies refs_file_w and refs_file_r should also be 0.
	KASSERT(pipe->refs_file_r == 0);
	KASSERT(pipe->refs_file_w == 0);
	
	//Nobody holds this pipe - clean it up
	kpage_free(pipe->buf_ptr, pipe->buf_len);
	pipe->buf_ptr = NULL;
	pipe->buf_len = 0;
	pipe->rptr = 0;
	pipe->wptr = 0;
	
}

//Returns how many more bytes can be read from the given pipe's buffer before it is empty.
static ssize_t pipe_canread(const pipe_t *p)
{
	ssize_t r = p->rptr;
	ssize_t w = p->wptr;
	if(w < r)
		w += p->buf_len;
	
	return w - r;
}

//Returns how many bytes can be read in a single shot from the pipe's buffer (without wrapping).
static ssize_t pipe_canread_single(const pipe_t *p)
{
	ssize_t to_buf = p->buf_len - p->rptr;
	ssize_t to_wptr = pipe_canread(p);
	
	return (to_wptr < to_buf) ? to_wptr : to_buf;
}

//Returns how many more bytes can be written to the given pipe's buffer before it is full.
static ssize_t pipe_canwrite(const pipe_t *p)
{
	ssize_t r = p->rptr;
	ssize_t w = p->wptr;
	if(w < r)
		w += p->buf_len;
	
	return (p->buf_len - 1) + r - w;
}

//Returns how many bytes can be written in a single shot to the pipe's buffer (without wrapping).
static ssize_t pipe_canwrite_single(const pipe_t *p)
{
	ssize_t to_buf = p->buf_len - p->wptr;
	ssize_t to_rptr = pipe_canwrite(p);
	
	return (to_rptr < to_buf) ? to_rptr : to_buf;
}

int pipe_new(pipe_t **pipe_out)
{
	//Find a spot for the pipe in the pipe table
	for(int pp = 0; pp < PIPE_MAX; pp++)
	{
		pipe_t *pptr = &(pipe_table[pp]);
		if(m_spl_try(&(pptr->spl)))
		{
			if(pptr->buf_len == 0)
			{
				size_t ll = 4096;
				pptr->buf_ptr = kpage_alloc(ll);
				if(pptr->buf_ptr == NULL)
				{
					//No room for buffer for this pipe.
					m_spl_rel(&(pptr->spl));
					return -ENOMEM;
				}
				pptr->buf_len = ll;
				
				pptr->id += PIPE_MAX;
				if( (pptr->id < 0) && ((pptr->id % PIPE_MAX) != pp) )
					pptr->id = pp;
				
				pptr->rptr = 0;
				pptr->wptr = 0;
				pptr->refs_ino = 0;
				pptr->refs_file_w = 0;
				pptr->refs_file_r = 0;
				
				*pipe_out = pptr;
				return 0; //Still locked
			}
			m_spl_rel(&(pptr->spl)); //Keep looking
		}
	}
	
	//No free spots
	return -ENFILE;
}

pipe_t *pipe_lockid(int id)
{
	if(id < 0)
		return NULL;
	
	pipe_t *pptr = &(pipe_table[id % PIPE_MAX]);
	m_spl_acq(&(pptr->spl));
	if(pptr->buf_ptr == NULL || pptr->id != id)
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
	pipe_checkrefs(pptr);
	m_spl_rel(&(pptr->spl));
}

ssize_t pipe_write(pipe_t *pptr, const void *buf, ssize_t nbytes)
{		
	//POSIX requires that writes of 512 bytes or less can complete atomically.
	//So just don't allow writing to pipes with less than 512 bytes in their buffer.
	if(pipe_canwrite(pptr) < 512)
	{
		if(pptr->refs_file_r)
			return -EAGAIN; //Pipe has no room, but has readers
		else
			return -EPIPE; //No room in the pipe and no one draining it
	}
		
	//Write as much as we can
	const char *buf_bytes = (const char*)buf;
	ssize_t written = 0;
	while(1)
	{
		ssize_t copylen = pipe_canwrite_single(pptr);
		if(copylen > nbytes)
			copylen = nbytes;
		
		if(copylen == 0)
			break;
		
		memcpy(pptr->buf_ptr + pptr->wptr, buf_bytes, copylen);
		pptr->wptr = (pptr->wptr + copylen) % (pptr->buf_len);
		
		buf_bytes += copylen;
		written += copylen;
		nbytes -= copylen;
	}
	return written;
}

ssize_t pipe_read(pipe_t *pptr, void *buf, ssize_t nbytes)
{	
	if(pipe_canread(pptr) < 1)
	{
		if(pptr->refs_file_w)
			return -EAGAIN; //Pipe empty, but has writers
		else
			return -EPIPE; //Pipe empty and no one open for writing
	}
	
	//Read as much as we can
	char *buf_bytes = (char*)buf;
	ssize_t nread = 0;
	while(1)
	{
		ssize_t copylen = pipe_canread_single(pptr);
		if(copylen > nbytes)
			copylen = nbytes;
		
		if(copylen == 0)
			break;
		
		memcpy(buf_bytes, pptr->buf_ptr + pptr->rptr, copylen);
		pptr->rptr = (pptr->rptr + copylen) % (pptr->buf_len);
		
		buf_bytes += copylen;
		nread += copylen;
		nbytes -= copylen;
	}
	return nread;
}

