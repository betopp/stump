//thread.c
//User threads in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "thread.h"
#include "m_intr.h"
#include "m_tls.h"
#include "kassert.h"
#include <errno.h>
#include <stddef.h>

thread_t thread_table[THREAD_MAX];

thread_t *thread_lockfree(void)
{
	for(int tt = 0; tt < THREAD_MAX; tt++)
	{
		//Don't bother fighting over a lock - locked thread is necessarily in use.
		thread_t *tptr = &(thread_table[tt]);
		if(m_spl_try(&(tptr->spl)))
		{
			if(tptr->state == THREAD_STATE_NONE)
			{
				//Found a free spot. 
				
				//Make sure it's got a valid ID
				tptr->tid += THREAD_MAX;
				if( (tptr->tid <= 0) || ((tptr->tid % THREAD_MAX) != tt) )
					tptr->tid = tt;
				
				//Return it, still locked.
				return tptr;
			}
			
			//Not free. Unlock and keep looking.
			m_spl_rel(&(tptr->spl));
		}
	}
	
	//Didn't find a spot.
	return NULL;
}

int thread_new(process_t *process, uintptr_t entry, thread_t **thread_out)
{
	thread_t *tptr = thread_lockfree();
	if(tptr == NULL)
	{
		//No room for new threads.
		return -ENOMEM;
	}
	
	tptr->state = THREAD_STATE_SUSPEND;
	tptr->process = process;
	
	m_drop_reset(&(tptr->drop), entry);
	
	process->nthreads++;
	
	//Success
	*thread_out = tptr;
	return 0;
}

thread_t *thread_locktid(id_t tid)
{
	if(tid < 0)
		return NULL;
	
	//Array index corresponds to ID
	thread_t *tptr = &(thread_table[tid % THREAD_MAX]);
	m_spl_acq(&(tptr->spl));
	
	if( (tptr->state == THREAD_STATE_NONE) || (tptr->tid != tid) )
	{
		//No/wrong thread here.
		m_spl_rel(&(tptr->spl));
		return NULL;
	}
	
	//Got the thread we wanted - return, still locked
	return tptr;
}

thread_t *thread_lockcur(void)
{
	thread_t *tptr = m_tls_get();
	m_spl_acq(&(tptr->spl));
	return tptr;
}

void thread_unlock(thread_t *thread)
{
	m_spl_rel(&(thread->spl));
}

void thread_unpause(id_t tid)
{
	//This kinda races but we don't care.
	//If the thread was cleaned-up or replaced then whatever, there's no harm in unpausing someone else.
	KASSERT(tid >= 0);
	m_atomic_increment_and_fetch(&(thread_table[tid % THREAD_MAX].unpauses));
}

void thread_sched(void)
{
	//Try until we get something to run.
	while(1)
	{
		//Disable interrupts while trying to schedule.
		//If a thread becomes runnable while we search, then, the resulting interrupt will be waiting for us.
		m_intr_ei(false);
		
		//Look for threads to run
		thread_t *tptr = NULL;
		for(int tt = 0; tt < THREAD_MAX; tt++)
		{
			if(m_spl_try(&(thread_table[tt].spl)))
			{
				if(thread_table[tt].state == THREAD_STATE_SUSPEND)
				{
					if(thread_table[tt].unpauses >= thread_table[tt].unpauses_req)
					{
						tptr = &(thread_table[tt]);
						break;
					}
				}
				
				m_spl_rel(&(thread_table[tt].spl));
			}
		}
		
		if(tptr == NULL)
		{
			//No threads runnable right now.
			//Wait for an interprocessor interrupt that might indicate something to do.
			m_intr_halt();
			
			//Try again to find a runnable thread.
			continue;
		}
		
		//Got a thread, it's ready, and we've locked it.
		
		//Note which thread we'll be running on this core, as its kernel stack/context is about to be clobbered.
		m_tls_set(tptr);
		
		//Mark the thread as running, and resume its userspace.
		//(Assume nobody's messing with this, if the thread is marked "running", even though we release the lock)
		tptr->state = THREAD_STATE_RUN;
		thread_unlock(tptr);
		
		m_uspc_activate(tptr->process->mem.uspc);
		m_drop(&(tptr->drop));
		
		//m_drop doesn't return.
		//When we re-enter the kernel, though, we'll know which thread triggered it, thanks to the TLS pointer.
	}
}