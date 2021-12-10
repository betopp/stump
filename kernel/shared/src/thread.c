//thread.c
//User threads in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "thread.h"
#include <errno.h>
#include <stddef.h>

//All threads in the system
#define THREAD_MAX 1024
static thread_t thread_table[THREAD_MAX];

//Finds a free entry in the thread table and returns it, locked.
static thread_t *thread_lockfree(void)
{
	for(int tt = 0; tt < THREAD_MAX; tt++)
	{
		//Don't bother fighting over a lock - locked thread is necessarily in use.
		thread_t *tptr = &(thread_table[tt]);
		if(m_spl_try(&(tptr->spl)))
		{
			if(tptr->state == THREAD_STATE_NONE)
			{
				//Found a free spot. Return it, still locked.
				return tptr;
			}
			
			//Not free. Unlock and keep looking.
			m_spl_rel(&(tptr->spl));
		}
	}
	
	//Didn't find a spot.
	return NULL;
}

int thread_new(process_t *process, thread_t **thread_out)
{
	thread_t *tptr = thread_lockfree();
	if(tptr == NULL)
	{
		//No room for new threads.
		return -ENOMEM;
	}
	
	tptr->state = THREAD_STATE_READY;
	tptr->process = process;
	process->nthreads++;
	
	//Success
	*thread_out = tptr;
	return 0;
}

void thread_unlock(thread_t *thread)
{
	m_spl_rel(&(thread->spl));
}

void thread_sched(void)
{
	//Look for threads to run
	thread_t *tptr = NULL;
	for(int tt = 0; tt < THREAD_MAX; tt++)
	{
		if(m_spl_try(&(thread_table[tt].spl)))
		{
			if(thread_table[tt].state == THREAD_STATE_READY)
			{
				tptr = &(thread_table[tt]);
				break;
			}
			
			m_spl_rel(&(thread_table[tt].spl));
		}
	}
		
	//Got a thread, it's ready, and we've locked it.
	
	//Mark the thread as running, and resume its userspace.
	tptr->state = THREAD_STATE_RUN;
	thread_unlock(tptr);
	
	m_drop(&(tptr->drop)); //Assume nobody's messing with this, if the thread is marked "running"
	
	//Eventually we'll come back here. See why we returned.
	while(1) { }
}