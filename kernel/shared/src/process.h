//process.h
//Process tracking in kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef PROCESS_H
#define PROCESS_H

#include "m_spl.h"
#include "m_uspc.h"
#include <sys/types.h>
#include <stdint.h>

//Process control block
typedef struct process_s
{
	//Spinlock protecting the process control block
	m_spl_t spl;
	
	//ID of the process
	pid_t pid;
	
	//Number of threads that belong to this process
	int64_t nthreads;
	
	//Userspace paging structures (probably: pointer to root table)
	m_uspc_t uspc;
	
} process_t;

//Sets up process tracking and initial process entry.
void process_init(void);

//Releases the lock on the given process.
void process_unlock(process_t *process);

#endif //PROCESS_H
