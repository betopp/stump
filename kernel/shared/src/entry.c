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
	//Save context in thread that was running and mark it as being serviced
	thread_t *tptr = thread_lockcur();
	
	KASSERT(tptr->state == THREAD_STATE_RUN);
	thread_chstate(tptr, THREAD_STATE_SYSCALL);
	m_drop_copy(&(tptr->drop), drop);
	
	thread_unlock(tptr);
	tptr = NULL;
	
	//Run the requested system call
	uintptr_t result = syscalls_handle(num, p1, p2, p3, p4, p5);
	
	//Put the result in the calling thread.
	//Hack - don't overwrite return value if returning from signal (interrupted context is already restored)
	if(num != 0x83)
	{
		tptr = thread_lockcur();
		m_drop_retval(&(tptr->drop), result);
		thread_unlock(tptr);
	}
	
	
	//Pick another thread to run
	thread_sched();
}

//Entered in interrupt context when a keyboard key is pressed or released.
//Should return, to return from interrupt service.
void entry_isr_kbd(_sc_con_scancode_t scancode, bool state)
{
	con_isr_kbd(scancode, state);
}
