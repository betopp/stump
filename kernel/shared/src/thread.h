//thread.h
//User threads in kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef THREAD_H
#define THREAD_H

#include "m_spl.h"
#include "m_drop.h"
#include "process.h"

//State a thread can be in
typedef enum thread_state_e
{
	THREAD_STATE_NONE = 0, //None/invalid
	
	THREAD_STATE_READY, //Thread ready to use the CPU if available, can be scheduled
	THREAD_STATE_RUN, //Thread is currently executing on some core
	THREAD_STATE_WAIT, //Thread is waiting for something, can't utilize CPU currently
	THREAD_STATE_DEAD, //Thread has exited, but status is not delivered to its parent yet
	
	THREAD_STATE_MAX
	
} thread_state_t;

//Thread control block
typedef struct thread_s
{
	//Spinlock protecting the thread control block
	m_spl_t spl;
	
	//State of thread
	thread_state_t state;
	
	//Process containing the thread
	process_t *process;
	
	//User context
	m_drop_t drop;
	
} thread_t;

//Makes a new thread. Outputs a pointer to it, still locked.
//Returns 0 on success or a negative error number.
int thread_new(process_t *process, uintptr_t entry, thread_t **thread_out);

//Unlocks the given thread.
void thread_unlock(thread_t *thread);

//Finds a runnable thread and runs it.
void thread_sched(void);


#endif //THREAD_H
