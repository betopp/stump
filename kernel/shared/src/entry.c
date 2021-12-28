//entry.c
//Entry points to kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "kpage.h"
#include "ramfs.h"
#include "fb.h"
#include "systar.h"
#include "process.h"
#include "thread.h"
#include "con.h"
#include "syscalls.h"
#include "m_panic.h"
#include "kassert.h"
#include <string.h>

//Entered once on bootstrap core. Should set up kernel and return.
void entry_boot(void)
{
	//Set up in-memory systems
	kpage_init();
	fb_init();
	ramfs_init();
	
	//Unpack and free the TAR file containing initial FS contents
	systar_unpack();
	
	//Make initial process to execute init
	process_init();
}

//Entered on all cores once entry_one returns. Should schedule threads and never return.
void entry_smp(void)
{
	//Schedule the first thread.
	//We'll re-enter the kernel somewhere else after it runs.
	thread_sched();
	m_panic("thread_sched returned");
}

//Entered on system-call. Should pick a user context to drop to and never return.
uintptr_t entry_syscall(uintptr_t num, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, uintptr_t p5, m_drop_t *drop)
{
	//Save context in thread that was running
	thread_t *tptr = thread_lockcur();
	KASSERT(tptr->state == THREAD_STATE_RUN);
	m_drop_copy(&(tptr->drop), drop);
	//Todo - do we move the thread to a different state? state_kernel or something?
	thread_unlock(tptr);
	tptr = NULL;
	
	//Run the requested system call
	uintptr_t result = syscalls_handle(num, p1, p2, p3, p4, p5);
	
	//If the process is dying, the thread is dead too
	process_t *pptr = process_lockcur();
	bool process_cleanup = (pptr->state != PROCESS_STATE_ALIVE);
	process_unlock(pptr);
	if(process_cleanup)
	{
		tptr = thread_lockcur();
		tptr->state = THREAD_STATE_DEAD;
		thread_unlock(tptr);
		tptr = NULL;
	}
	
	//Handle the result in the calling thread...
	tptr = thread_lockcur();
	
	//Hack - don't overwrite return value when returning from signal
	if(num != 0x83)
		m_drop_retval(&(tptr->drop), result);
	
	if(tptr->state == THREAD_STATE_DEAD)
	{
		//Thread has exited - clean it up and maybe clean up its process too.,
		m_uspc_activate(0);
		thread_cleanup(tptr);
		thread_unlock(tptr);
		
		thread_sched();
		m_panic("thread_sched returned");
	}
	
	if(tptr->sigpend & ~(tptr->sigmask))
	{
		//Thread is being signalled.
		
		//Figure out which signal to trigger.
		int64_t triggered = tptr->sigpend & ~(tptr->sigmask);
		int signum = -1;
		for(int bb = 0; bb < 64; bb++)
		{
			if(triggered & (1 << bb))
			{
				signum = bb;
				break;
			}
		}
		KASSERT(signum >= 0 && signum < 63);
		tptr->siginfo.signum = signum;
		
		//Set aside info about thread at the time it was signalled
		m_drop_copy(&(tptr->sigdrop), &(tptr->drop));
		tptr->siginfo.mask = tptr->sigmask;
		
		//Mask further signals
		tptr->sigmask = 0x7FFFFFFFFFFFFFFFul;
		
		//Continue at specified signal handler address
		m_drop_signal(&(tptr->drop), tptr->sigpc, tptr->sigsp);

		//Thread unpauses when signalled, of course
		tptr->unpauses++;
		
		//Signal is no longer pending
		tptr->sigpend &= ~(1u << signum);
	}
	
	if(tptr->unpauses < tptr->unpauses_req)
	{
		//Thread has consumed all existing unpauses (i.e. it paused).
		//Schedule something else until it is next unpaused.
		tptr->state = THREAD_STATE_SUSPEND;
		m_uspc_activate(0);
		thread_unlock(tptr);
		
		thread_sched();
		m_panic("thread_sched returned");
	}

	//If the calling thread hasn't paused, just continue running it.
	//We could actually make a scheduling decision here, too... todo?
	tptr->state = THREAD_STATE_RUN;
	thread_unlock(tptr);
	
	m_drop(&(tptr->drop));
}

//Entered in interrupt context when a keyboard key is pressed or released.
//Should return, to return from interrupt service.
void entry_kbd(_sc_con_scancode_t scancode, bool state)
{
	con_isr_kbd(scancode, state);
}
