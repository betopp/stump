//syscalls_sig.c
//System call declarations on kernel-side - signal handling
//Bryan E. Topp <betopp@betopp.com> 2021

#include "syscalls.h"
#include "thread.h"
#include "kassert.h"
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

void k_sc_sig_entry(uintptr_t pc, uintptr_t sp)
{
	thread_t *tptr = thread_lockcur();
	tptr->sigpc = pc;
	tptr->sigsp = sp;
	thread_unlock(tptr);
}

int64_t k_sc_sig_mask(int how, int64_t mask)
{
	thread_t *tptr = thread_lockcur();
	int64_t retval = tptr->sigmask;
	switch(how)
	{
		case SIG_BLOCK:
		{
			tptr->sigmask |= mask;
			break;
		}
		case SIG_SETMASK:
		{
			tptr->sigmask = mask;
			break;
		}
		case SIG_UNBLOCK:
		{
			tptr->sigmask &= ~mask;
			break;
		}
		default:
		{
			thread_unlock(tptr);
			return -EINVAL;
		}
	}
	tptr->sigmask &= 0x7FFFFFFFFFFFFFFFul;
	tptr->sigmask &= ~(1ul << SIGKILL);
	tptr->sigmask &= ~(1ul << SIGSTOP);
	thread_unlock(tptr);
	KASSERT(retval >= 0);
	return retval;
}

ssize_t k_sc_sig_info(_sc_sig_info_t *buf_ptr, ssize_t buf_len)
{
	if(buf_len < 1)
		return -EINVAL;
	
	thread_t *tptr = thread_lockcur();
	_sc_sig_info_t siginfo = tptr->siginfo;
	thread_unlock(tptr);
	tptr = NULL;
	
	if(buf_len > (ssize_t)sizeof(siginfo))
		buf_len = sizeof(siginfo);
	
	int copy_err = process_memput(buf_ptr, &siginfo, buf_len);
	if(copy_err < 0)
		return copy_err;
	
	return buf_len;
}

void k_sc_sig_return(void)
{
	thread_t *tptr = thread_lockcur();
	
	//Restore interrupted user context and mask
	m_drop_copy(&(tptr->drop), &(tptr->sigdrop));
	tptr->sigmask = tptr->siginfo.mask;
	
	//Clear info about handled signal
	memset(&(tptr->sigdrop), 0, sizeof(tptr->sigdrop));
	memset(&(tptr->siginfo), 0, sizeof(tptr->siginfo));
	
	thread_unlock(tptr);
}

int k_sc_sig_send(int idtype, int id, int sig)
{
	if(sig < 0 || sig >= 63)
		return -EINVAL;
	
	bool any_signal = false;
	for(int tt = 0; tt < THREAD_MAX; tt++)
	{
		thread_t *tptr = &(thread_table[tt]);
		m_spl_acq(&(tptr->spl));
		
		if(tptr->state == THREAD_STATE_NONE)
		{
			//No thread here
			m_spl_rel(&(tptr->spl));
			continue;
		}
		
		bool id_match = false;
		switch(idtype)
		{
			case P_PID: id_match = (tptr->process->pid == id); break;
			case P_PGID: id_match = (tptr->process->pgid == id); break;
			case P_TID: id_match = (tptr->tid == id); break;
			case P_ALL: id_match = true; break;
			default: m_spl_rel(&(tptr->spl)); return -EINVAL;
		}
		
		if(!id_match)
		{
			//Not a thread we're looking for
			m_spl_rel(&(tptr->spl));
			continue;
		}
		
		//Okay, this is a thread we want to signal. We'll consider this a success.
		any_signal = true;
		tptr->sigpend |= (1ul << sig);
		tptr->siginfo.pid = tptr->process->pid;
		tptr->siginfo.tid = tptr->tid;
		thread_unpause(tptr->tid);
		
		m_spl_rel(&(tptr->spl));
		
		//PID and TID options signal a single thread; others signal multiple.
		if(idtype == P_PID || idtype == P_TID)
			break;
	}
	
	return any_signal ? 0 : -ESRCH;
}