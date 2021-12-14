//process.h
//Process tracking in kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef PROCESS_H
#define PROCESS_H

#include "m_spl.h"
#include "m_uspc.h"
#include "file.h"
#include "mem.h"
#include <sys/types.h>
#include <stdint.h>

//States a process can be in
typedef enum process_state_e
{
	PROCESS_STATE_NONE = 0,
	
	PROCESS_STATE_ALIVE,
	PROCESS_STATE_DEAD,
	
	PROCESS_STATE_MAX
	
} process_state_t;

//File descriptor in a process
typedef struct process_fd_s
{
	//File described by this descriptor
	file_t *file;
	
	//Flags about the descriptor (i.e. close-on-exec or not)
	int flags;
	
} process_fd_t;

//Process control block
typedef struct process_s
{
	//Spinlock protecting the process control block
	m_spl_t spl;
	
	//State of process
	process_state_t state;
	
	//ID of the process
	pid_t pid;
	
	//ID of the parent of the process
	pid_t ppid;
	
	//Process group ID
	pid_t pgid;
	
	//Number of threads that belong to this process
	int64_t nthreads;
	
	//File descriptors
	#define PROCESS_FD_MAX 128
	process_fd_t fds[PROCESS_FD_MAX];
	
	//Present working directory
	file_t *pwd;
	
	//Memory space that threads in this process use.
	mem_t mem;
	
	//Spare memory space if we're doing an exec, in case we fail.
	mem_t mem_attempt;
	
	//Status information available for parent to wait() on
	int wstatus;
	
} process_t;

//All processes on system
#define PROCESS_MAX 256
extern process_t process_table[PROCESS_MAX];

//Sets up process tracking and initial process entry.
void process_init(void);

//Locks an empty entry in the process table and returns a pointer to it.
process_t *process_lockfree(void);

//Locks and returns a pointer to a process by PID. Returns NULL if it doesn't exist.
process_t *process_lockpid(pid_t pid);

//Locks the current process and returns a pointer to it.
process_t *process_lockcur(void);

//Releases the lock on the given process.
void process_unlock(process_t *process);

//Looks up a file descriptor in the current process and locks the file.
//Returns NULL if no file is present there.
//Optionally allows specifying the PWD by passing -1.
file_t *process_lockfd(int fd, bool allow_pwd);

//Attempts to insert a file into the current process. Returns its file descriptor number or a negative error number.
//Does not alter the file's reference count.
int process_addfd(file_t *newfile);

//Attempts to copy a string from the current process into the kernel.
int process_strget(char *kbufptr, const char *uptr, size_t kbuflen);

//Attempts to copy a buffer from the kernel into the current process.
int process_memput(void *ubufptr, const void *kbufptr, size_t len);

#endif //PROCESS_H
