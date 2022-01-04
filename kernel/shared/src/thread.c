//thread.c
//User threads in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "thread.h"
#include "m_intr.h"
#include "m_tls.h"
#include "kassert.h"
#include "con.h"
#include "kpage.h"
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/wait.h>

thread_t thread_table[THREAD_MAX];

thread_t *thread_lockfree(void)
{
	for(int tt = 1; tt < THREAD_MAX; tt++)
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
	
	//Threads start with all signals masked, so they don't catch them before they're ready.
	tptr->sigmask = 0x7FFFFFFFFFFFFFFFul;
	tptr->sigpend = 0;
	
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
	//CAN BE CALLED FROM ISR.
	
	//This kinda races but we don't care.
	//If the thread was cleaned-up or replaced then whatever, there's no harm in unpausing someone else.
	KASSERT(tid >= 0);
	m_atomic_increment_and_fetch(&(thread_table[tid % THREAD_MAX].unpauses));
	
	//Send interprocessor interrupt after incrementing the unpauses count.
	//This way, anyone who saw the old count will receive a pending interrupt after they see it.
	//They'll have interrupts disabled while looking at the old count - so they'll wake immediately when they try to sleep.
	m_intr_wake();
}

id_t thread_curtid(void)
{
	thread_t *tptr = m_tls_get();
	return 	tptr->tid;
}

void thread_cleanup(thread_t *tptr)
{
	//Set aside process that contained the thread
	pid_t oldpid = tptr->process->pid;
	
	//Clear thread structure
	tptr->state = THREAD_STATE_NONE;
	tptr->process = NULL;
	memset(&(tptr->drop), 0, sizeof(tptr->drop));
	memset(&(tptr->sigdrop), 0, sizeof(tptr->sigdrop));
	tptr->sigmask = 0;
	tptr->sigpend = 0;
	tptr->unpauses = 0;
	tptr->unpauses_req = 0;
	thread_unlock(tptr);
	
	//Reduce the thread-count of the process that the thread was a part of.
	process_t *pptr = process_lockpid(oldpid);
	KASSERT(pptr != NULL);
	
	KASSERT(pptr->nthreads > 0);
	pptr->nthreads--;
	
	//If the process has no threads, it's dead all the way
	if(pptr->nthreads == 0)
	{
		//Track whether we're exiting while holding the console - give it back to PID1 if so.
		bool hadcon = false;
		
		//Clean up the process.
		m_uspc_activate(0);
		mem_clear(&(pptr->mem));
		mem_clear(&(pptr->mem_attempt));
		
		for(int ff = 0; ff < PROCESS_FD_MAX; ff++)
		{
			if(pptr->fds[ff].file != NULL)
			{
				file_lock(pptr->fds[ff].file);
				pptr->fds[ff].file->refs--;
				file_unlock(pptr->fds[ff].file);
			}
			memset(&(pptr->fds[ff]), 0, sizeof(pptr->fds[ff]));
		}
		
		if(pptr->pwd != NULL)
		{
			file_lock(pptr->pwd);
			pptr->pwd->refs--;
			file_unlock(pptr->pwd);
		}
		pptr->pwd = NULL;
		
		hadcon = pptr->hascon;
		pptr->hascon = false;
		pptr->contid = 0;

		if(pptr->fb.bufptr != NULL)
			kpage_free(pptr->fb.bufptr, pptr->fb.buflen);
		
		memset(&(pptr->fb), 0, sizeof(pptr->fb));
		
		//Somebody should have called exit() on the process, rather than just killing all its threads.
		//But, if all threads died and none wanted to kill the process, just exit with code 0.
		if(pptr->wstatus == 0)
			pptr->wstatus = _WIFEXITED_FLAG;
		
		//Process is now dead, and only exists to be waited-upon by its parent.
		pptr->state = PROCESS_STATE_DEAD;
		pid_t ppid = pptr->ppid;
		process_unlock(pptr);
		pptr = NULL;
		
		//Unpause all threads in the parent process, so one can wait on the now-dead child process
		for(int tt = 0; tt < THREAD_MAX; tt++)
		{
			m_spl_acq(&(thread_table[tt].spl));
			if(thread_table[tt].state != THREAD_STATE_NONE)
			{
				if(thread_table[tt].process->pid == ppid)
				{
					thread_table[tt].sigpend |= (1u << SIGCHLD);
					thread_unpause(thread_table[tt].tid);
				}
			}
			m_spl_rel(&(thread_table[tt].spl));
		}
		
		//If the process with the console just died, the console returns to PID 1.
		if(hadcon)
			con_steal_prepare();
	}
	else
	{
		//Process still has threads - don't clean it up yet.
		process_unlock(pptr);
	}
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
		KASSERT(tptr->state == THREAD_STATE_SUSPEND);
		tptr->state = THREAD_STATE_RUN;
		thread_unlock(tptr);
		
		m_uspc_activate(tptr->process->mem.uspc);
		m_drop(&(tptr->drop));
		
		//m_drop doesn't return.
		//When we re-enter the kernel, though, we'll know which thread triggered it, thanks to the TLS pointer.
	}
}