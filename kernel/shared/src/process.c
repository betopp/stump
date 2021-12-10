//process.c
//Process tracking in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "process.h"
#include "thread.h"
#include "kassert.h"
#include <errno.h>
#include <stddef.h>

//All processes on system
#define PROCESS_MAX 256
static process_t process_table[PROCESS_MAX];

void process_init(void)
{
	//Make initial process entry
	process_t *pptr = &(process_table[1]);
	m_spl_acq(&(pptr->spl));
	
	pptr->pid = 1;
	pptr->uspc = m_uspc_new();
	KASSERT(pptr->uspc != 0);
	
	//Make thread to run initial process
	thread_t *init_thread = NULL;
	int thread_err = thread_new(pptr, &init_thread);
	KASSERT(thread_err == 0);
	
	thread_unlock(init_thread);
	process_unlock(pptr);
}

void process_unlock(process_t *process)
{
	m_spl_rel(&(process->spl));
}