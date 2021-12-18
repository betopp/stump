//pipe.h
//Pipes in kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef PIPE_H
#define PIPE_H

#include <sys/types.h>
#include <stdint.h>
#include "m_spl.h"

//Information about one pipe
typedef struct pipe_s
{
	//Spinlock protecting the pipe data
	m_spl_t spl;
	
	//ID of pipe
	int id;
	
	//Buffer for pipe data
	uint8_t *buf_ptr;
	size_t buf_len;
	
	//Next read location in buffer
	int rptr;
	
	//Next write location in buffer
	int wptr;
	
	//Number of inodes referring to the pipe as a named pipe
	int refs_ino;
	
	//Number of files referring to the pipe - though perhaps neither open-for-read nor open-for-write
	int refs_file;
	
	//Number of files with the pipe open for read/write (needed, so we can return EPIPE appropriately)
	int refs_file_w;
	int refs_file_r;
	
} pipe_t;

//Makes a new pipe and outputs its location, still locked.
//Returns 0 on success or a negative error number.
int pipe_new(pipe_t **pipe_out);

//Looks up and locks the pipe with the given ID.
pipe_t *pipe_lockid(int id);

//Unlocks the given pipe.
void pipe_unlock(pipe_t *pptr);

//Tries to write into the given pipe. Returns the number of bytes written, or a negative error number.
ssize_t pipe_write(pipe_t *pptr, const void *buf, ssize_t len);

//Tries to read from the given pipe. Returns the number of bytes read, or a negative error number.
ssize_t pipe_read(pipe_t *pptr, void *buf, ssize_t len);

#endif //PIPE_H
