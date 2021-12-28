//sc_sig.h
//System call library - signal handling
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef _SC_SIG_H
#define _SC_SIG_H

//Sets the program counter and stack pointer that are loaded when a signal is taken.
//When a signal is taken:
// The old pc/sp/mask are preserved, to be queried with _sc_siginfo
// The new pc/sp specified here are loaded
// All signals are masked (mask bits all set to 1)
void _sc_sig_entry(uintptr_t pc, uintptr_t sp);

//Alters the signal mask of the calling process. Returns the old mask.
int64_t _sc_sig_mask(int how, int64_t mask);

//Information about why and where a signal handler was executed.
typedef struct _sc_siginfo_s
{
	//Signal number that was received
	int signum;
	
	//Address of fault, if any
	uintptr_t fault;
	
	//Sender of signal, if any
	pid_t pid;
	id_t tid;
	
	//Signal mask at the time the signal was taken.
	//When a handler is executed, all signals are masked so the process can safely enter/exit the handler.
	int64_t mask;
	
} _sc_sig_info_t;

//Returns information about why the latest signal handler was executed.
ssize_t _sc_sig_info(_sc_sig_info_t *buf_ptr, ssize_t buf_len);

//Returns to the user-mode state that was interrupted by the last signal taken.
void _sc_sig_return(void) __attribute__((noreturn));

//Sends a signal.
int _sc_sig_send(int idtype, int id, int sig);

#endif //_SC_SIG_H
