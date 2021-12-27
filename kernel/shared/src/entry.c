//entry.c
//Entry points to kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "kpage.h"
#include "ramfs.h"
#include "fb.h"
#include "systar.h"
#include "process.h"
#include "thread.h"
#include "syscalls.h"
#include "m_panic.h"
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
	m_drop_copy(&(tptr->drop), drop);
	//Todo - do we move the thread to a different state? state_kernel or something?
	thread_unlock(tptr);
	
	//Run the requested system call
	uintptr_t result = syscalls_handle(num, p1, p2, p3, p4, p5);
	
	//Handle the result in the calling thread...
	tptr = thread_lockcur();	
	if(tptr->state == THREAD_STATE_DEAD)
	{
		//Thread is dead - clean it up
		while(1) { }
	}
	else if(tptr->unpauses < tptr->unpauses_req)
	{
		//Thread has consumed all existing unpauses (i.e. it paused).
		//Schedule something else until it is next unpaused.
		tptr->state = THREAD_STATE_SUSPEND;
		thread_unlock(tptr);
		
		thread_sched();
		m_panic("thread_sched returned");
	}
	else
	{
		//If the calling thread hasn't paused, just continue running it.
		//We could actually make a scheduling decision here, too... todo?
		tptr->state = THREAD_STATE_RUN;
		thread_unlock(tptr);
		
		m_drop_retval(&(tptr->drop), result);
		m_drop(&(tptr->drop));
	}
}
