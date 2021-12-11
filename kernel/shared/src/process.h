//process.h
//Process tracking in kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef PROCESS_H
#define PROCESS_H

#include "m_spl.h"
#include "m_uspc.h"
#include "file.h"
#include "mem.h"
#include <sys/types.h>
#include <stdint.h>

//File descriptor in a process
typedef struct process_fd_s
{
	//File described by this descriptor
	file_t *file;
	
	//Flags about the descriptor (i.e. close-on-exec or not)
	int flags;
	
} process_fd_t;

//Process control block
typedef struct process_s
{
	//Spinlock protecting the process control block
	m_spl_t spl;
	
	//ID of the process
	pid_t pid;
	
	//Number of threads that belong to this process
	int64_t nthreads;
	
	//File descriptors
	#define PROCESS_FD_MAX 128
	process_fd_t fds[PROCESS_FD_MAX];
	
	//Memory space that threads in this process use.
	mem_t mem;
	
	//Spare memory space if we're doing an exec, in case we fail.
	mem_t mem_attempt;
	
} process_t;

//Sets up process tracking and initial process entry.
void process_init(void);

//Releases the lock on the given process.
void process_unlock(process_t *process);



#endif //PROCESS_H
