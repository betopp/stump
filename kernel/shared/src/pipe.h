//pipe.h
//Pipes in kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef PIPE_H
#define PIPE_H

#include <sys/types.h>
#include <stdint.h>
#include "m_spl.h"

//Our pipes are a bit more complex than a typical *NIX.
//We use them to implement pseudoterminals for our graphical terminal.
//So, they need to be bidirectional and have some process-group stuff.

//Directions supported in pipes
typedef enum pipe_dir_e
{
	PIPE_DIR_NONE = 0, //None/invalid
	
	PIPE_DIR_FORWARD, //Normal unidirectional pipe, as in POSIX spec
	PIPE_DIR_REVERSE, //Backward data, used in pseudoterminals
	
	PIPE_DIR_MAX
} pipe_dir_t;

//One direction in one pipe
typedef struct pipe_dirinfo_s
{
	//Buffer for pipe data
	uint8_t *buf_ptr;
	size_t buf_len;
	
	//Next read location in buffer
	int rptr;
	
	//Next write location in buffer
	int wptr;
	
	//Number of files with this direction open for read/write (needed, so we can return EPIPE appropriately)
	int refs_w;
	int refs_r;
	
	//Threads that get unblocked on another thread accessing the pipe
	#define PIPE_WAITING_MAX 8
	id_t waiting_to_w[PIPE_WAITING_MAX];
	id_t waiting_to_r[PIPE_WAITING_MAX];
	
} pipe_dirinfo_t;

//Information about one pipe
typedef struct pipe_s
{
	//Spinlock protecting the pipe data
	m_spl_t spl;
	
	//ID of pipe - always positive, because files refer to negative IDs as "reverse direction"
	int id;
	
	//Information per pipe direction
	pipe_dirinfo_t dirs[PIPE_DIR_MAX];
	
} pipe_t;

//Makes a new pipe and outputs its location, still locked.
//Returns 0 on success or a negative error number.
int pipe_new(pipe_t **pipe_out);

//Cleans up the given pipe and unlocks it.
void pipe_delete(pipe_t *pipe);

//Looks up and locks the pipe with the given ID.
pipe_t *pipe_lockid(int id);

//Unlocks the given pipe.
void pipe_unlock(pipe_t *pptr);

//Tries to write into the given pipe. Returns the number of bytes written, or a negative error number.
ssize_t pipe_write(pipe_t *pptr, pipe_dir_t dir, const void *buf, ssize_t len);

//Tries to read from the given pipe. Returns the number of bytes read, or a negative error number.
ssize_t pipe_read(pipe_t *pptr, pipe_dir_t dir, void *buf, ssize_t len);

#endif //PIPE_H
