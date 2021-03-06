//thread.h
//User threads in kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef THREAD_H
#define THREAD_H

#include "m_spl.h"
#include "m_drop.h"
#include "m_atomic.h"
#include "process.h"

#include <sys/types.h>
#include <sc.h>

//State a thread can be in
typedef enum thread_state_e
{
	THREAD_STATE_NONE = 0, //None/invalid
	
	THREAD_STATE_SUSPEND, //Thread is not executing - may be scheduled unless waiting for unpauses
	THREAD_STATE_RUN, //Thread is currently executing on some core
	THREAD_STATE_SYSCALL, //Thread is having a system-call serviced
	THREAD_STATE_DEAD, //Thread has exited, but status is not delivered to its parent yet
	
	THREAD_STATE_MAX
	
} thread_state_t;

//Thread control block
typedef struct thread_s
{
	//Spinlock protecting the thread control block
	m_spl_t spl;
	
	//ID of the thread
	id_t tid;
	
	//State of thread control block
	thread_state_t state;
	
	
	//How much time has been spent in each state
	int64_t tsc_totals[THREAD_STATE_MAX];
	
	//Timestamp when state was last changed
	int64_t tsc_last;
	
	//Timestamp at which we'll make another scheduling decision
	int64_t tsc_resched;
	
	
	//Process containing the thread
	process_t *process;
	
	//User context
	m_drop_t drop;
	
	
	//Context preserved when a signal is taken
	m_drop_t sigdrop;
	
	//Signals blocked (1 = blocked)
	int64_t sigmask;
	
	//Signals pending (1 = pending)
	int64_t sigpend;
	
	//Program counter to load on signal
	uintptr_t sigpc;
	
	//Stack pointer to load on signal
	uintptr_t sigsp;
	
	//Information about the thread just before a signal handler was entered
	_sc_sig_info_t siginfo;
	
	
	//Number of times the thread has been unpaused
	m_atomic_t unpauses;
	
	//What value of "unpauses" is sufficient to continue executing the thread
	m_atomic_t unpauses_req;

	
} thread_t;

//All threads in the system
#define THREAD_MAX 1024
extern thread_t thread_table[THREAD_MAX];

//Makes a new thread. Outputs a pointer to it, still locked.
//Returns 0 on success or a negative error number.
int thread_new(process_t *process, uintptr_t entry, thread_t **thread_out);

//Locks a free entry in the thread table and returns a pointer to it.
thread_t *thread_lockfree(void);

//Locks the thread with the given TID, if it exists, and returns a pointer to it.
//Returns NULL if the thread doesn't exist.
thread_t *thread_locktid(id_t tid);

//Locks the current thread and returns a pointer to it.
thread_t *thread_lockcur(void);

//Unlocks the given thread.
void thread_unlock(thread_t *thread);

//Changes the state of the given thread, updating its accounting
void thread_chstate(thread_t *thread, thread_state_t newstate);

//Causes a thread to resume if waiting, or skip its next waiting if already ready.
//CAN BE CALLED FROM ISR.
void thread_unpause(id_t tid);

//Returns the thread ID of the current thread.
id_t thread_curtid(void);

//Cleans up the given thread.
void thread_cleanup(thread_t *thread);

//Finds a runnable thread and runs it, waiting until one is runnable if necessary.
//Does not return after running the thread.
void thread_sched(void) __attribute__((noreturn));

#endif //THREAD_H
