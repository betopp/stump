//signal.c
//Signal-related functions for libc
//Bryan E. Topp <betopp@betopp.com> 2021

#include <signal.h>
#include <errno.h>
#include <tls.h>
#include <stddef.h>
#include <tls.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sc.h>
#include <sys/wait.h>

const char * const sys_siglist[_SIG_MAX] = 
{
	[SIGZERO] = "Signal Zero",
	[SIGHUP] = "Hangup",
	[SIGINT] = "Interrupt",
	[SIGQUIT] = "Quit",
	[SIGILL] = "Illegal Instruction",
	[SIGTRAP] = "Debug Trap",
	[SIGABRT] = "Abort",
	[SIGEMT] = "Emulation",
	[SIGFPE] = "Floating Point Exception",
	[SIGKILL] = "Killed",
	[SIGBUS] = "Bus Fault",
	[SIGSEGV] = "Segmentation Fault",
	[SIGSYS] = "Bad Syscall",
	[SIGPIPE] = "Broken Pipe",
	[SIGALRM] = "Alarm",
	[SIGTERM] = "Terminated",
	[SIGURG] = "Urgent",
	[SIGSTOP] = "Stopped",
	[SIGTSTP] = "Stopped by terminal",
	[SIGCONT] = "Continued",
	[SIGCHLD] = "Child Exited",
	[SIGTTIN] = "Input while background",
	[SIGTTOU] = "Output while background",
	[SIGXCPU] = "Exceeded CPU",
	[SIGXFSZ] = "Exceeded filesize",
	[SIGVTALRM] = "Virtual time alarm",
	[SIGPROF] = "Profiling timer",
	[SIGWINCH] = "Window change",
	[SIGINFO] = "Status request",
	[SIGUSR1] = "User signal 1",
	[SIGUSR2] = "User signal 2",
	[SIGTHR] = "Thread interrupt",
};

const char * const sys_signame[_SIG_MAX] = 
{
	[SIGZERO] = "ZERO",
	[SIGHUP] = "HUP",
	[SIGINT] = "INT",
	[SIGQUIT] = "QUIT",
	[SIGILL] = "ILL",
	[SIGTRAP] = "TRAP",
	[SIGABRT] = "ABRT",
	[SIGEMT] = "EMT",
	[SIGFPE] = "FPE",
	[SIGKILL] = "KILL",
	[SIGBUS] = "BUS",
	[SIGSEGV] = "SEGV",
	[SIGSYS] = "SYS",
	[SIGPIPE] = "PIPE",
	[SIGALRM] = "ALRM",
	[SIGTERM] = "TERM",
	[SIGURG] = "URG",
	[SIGSTOP] = "STOP",
	[SIGTSTP] = "TSTP",
	[SIGCONT] = "CONT",
	[SIGCHLD] = "CHLD",
	[SIGTTIN] = "TTIN",
	[SIGTTOU] = "TTOU",
	[SIGXCPU] = "XCPU",
	[SIGXFSZ] = "XFSZ",
	[SIGVTALRM] = "VTALRM",
	[SIGPROF] = "PROF",
	[SIGWINCH] = "WINCH",
	[SIGINFO] = "INFO",
	[SIGUSR1] = "USR1",
	[SIGUSR2] = "USR2",
	[SIGTHR] = "THR",
};

int sigaction(int sig, const struct sigaction *action, struct sigaction *oldaction)
{
	if(sig < 0 || sig >= 64)
	{
		errno = EINVAL;
		return -1;
	}
	
	_tls_t *tls = _tls();
	
	if(oldaction != NULL)
		*oldaction = tls->sigactions[sig];
	
	if(action != NULL)
		tls->sigactions[sig] = *action;
	
	return 0;
}

int sigaddset(sigset_t *set, int signo)
{
	if(signo < 0 || signo >= 64)
	{
		errno = EINVAL;
		return -1;
	}
	
	set->blockedbits |= 1 << signo;
	return 0;
}

int sigemptyset(sigset_t *set)
{
	set->blockedbits = 0;
	return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
	//FreeBSD says that threaded applications "must use" the pthreads version of this call.
	//I assume their implementation is like this - just change the current thread regardless.
	int64_t result;
	if(set != NULL)
	{
		result = _sc_sig_mask(how, set->blockedbits);
	}
	else
	{
		result = _sc_sig_mask(SIG_BLOCK, 0);
	}

	if(result < 0)
	{
		errno = -result;
		return -1;
	}
	
	if(oset != NULL)
	{
		oset->blockedbits = result;
	}
	
	return 0;
}

int sigsuspend(const sigset_t *sigmask)
{	
	//Callers already use this like we use _sc_pause - in a loop checking for a condition.
	//So briefly change the signal mask, calling pause once inbetween.
	//If the signal happens upon masking, or during pausing, we return either way.
	int64_t oldmask = _sc_sig_mask(SIG_SETMASK, sigmask->blockedbits);
	_sc_pause();
	_sc_sig_mask(SIG_SETMASK, oldmask);
	
	//We might have unpaused for any number of reasons.
	//Can a caller actually determine when no signal occurred?
	//The behavior of this function implies that they don't care.
	errno = EINTR;
	return -1;
}

int kill(pid_t pid, int sig)
{
	int result = -EINVAL;
	if(pid == -1)
		result = _sc_sig_send(P_ALL, 0, sig);
	else if(pid == 0)
		result = _sc_sig_send(P_PGID, getpgrp(), sig);
	else if(pid > 0)
		result = _sc_sig_send(P_PID, pid, sig);
	
	if(result < 0)
	{
		errno = -result;
		return -1;
	}
	
	return 0;
}

int killpg(pid_t pgrp, int sig)
{
	int result = _sc_sig_send(P_PGID, pgrp, sig);
	if(result < 0)
	{
		errno = -result;
		return -1;
	}
	
	return 0;
}

typedef void (*sig_t)(int);
sig_t signal(int sig, sig_t func)
{
	struct sigaction newaction =
	{
		.sa_handler = func,
		.sa_mask = {(1<<sig)},
		.sa_flags = 0,
		.sa_sigaction = NULL
	};
	
	struct sigaction oldaction = {0};
		
	int result = sigaction(sig, &newaction, &oldaction);
	if(result == -1)
		return SIG_ERR; //sigaction sets errno
	
	return oldaction.sa_handler;
}

int raise(int sig)
{
	int err = _sc_sig_send(P_PID, getpid(), sig);
	if(err < 0)
	{
		errno = -err;
		return -1;
	}
	return 0;
}

//Signal handler run in place of SIG_DFL
void _libc_signalled_dfl(int signum)
{
	switch(signum)
	{
		case SIGHUP:
		case SIGINT:
		case SIGKILL:
		case SIGPIPE:
		case SIGALRM:
		case SIGTERM:
		case SIGXCPU:
		case SIGXFSZ:
		case SIGVTALRM:
		case SIGPROF:
		case SIGUSR1:
		case SIGUSR2:
		case SIGTHR:
			//Should be "Terminate process"
			_sc_exit(0, signum);
		
		case SIGQUIT:
		case SIGILL:
		case SIGTRAP:
		case SIGABRT:
		case SIGEMT:
		case SIGFPE:
		case SIGBUS:
		case SIGSEGV:
		case SIGSYS:
			//Should be "Core dump"
			_sc_exit(0, signum);
		
		case SIGURG:
		case SIGCONT:
		case SIGCHLD:
		case SIGWINCH:
		case SIGINFO:
			//Ignore
			return;
		
		case SIGSTOP:
		case SIGTSTP:
		case SIGTTIN:
		case SIGTTOU:
			//Should be "stop process"
			return;
		
		default:
			return;
	}
}

//Signal handler run in place of SIG_IGN
void _libc_signalled_ign(int signum)
{
	(void)signum;
	return;
}

//Entry point when signalled - called by crt0
void _libc_signalled(void)
{
	//Figure out what signal we got
	_sc_sig_info_t siginfo = {0};
	ssize_t info_sz = _sc_sig_info(&siginfo, sizeof(siginfo));
	assert(info_sz > 0);
	
	//Call the appropriate handler
	if(siginfo.signum > 0 && siginfo.signum < 64)
	{
		_tls_t *tls = _tls();
		const struct sigaction *sa = &(tls->sigactions[siginfo.signum]);		
		if(sa->sa_sigaction != NULL)
		{
			assert(0); //Todo - call sigaction and support this stuff
		}
		else
		{
			if(sa->sa_handler == SIG_DFL)
			{
				_libc_signalled_dfl(siginfo.signum);
			}
			else if(sa->sa_handler == SIG_IGN)
			{
				_libc_signalled_ign(siginfo.signum);
			}
			else
			{
				(*(sa->sa_handler))(siginfo.signum);
			}
		}
	}
	
	//crt0 will return from the signal
	return;
}
