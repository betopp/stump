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

//Entered on system-call. Can return to continue executing the thread that made the system-call.
//Optionally, can ask for machine-specific code to save the user-context that entered here.
uintptr_t entry_syscall(uintptr_t num, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, uintptr_t p5)
{
	return syscalls_handle(num, p1, p2, p3, p4, p5);
}