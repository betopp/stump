//sc.h
//System call library
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef _SC_H
#define _SC_H

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

#include <sc_con.h>
#include <sc_mem.h>

//Does nothing.
void _sc_none(void);

//Terminates the calling process.
void _sc_exit(int exitcode, int signal) __attribute__((noreturn));

//Halts the machine, displaying the given error message if possible.
//Does not return on success. Returns a negative error number on failure.
//Fails primarily when the caller does not have authority to halt the machine.
int _sc_panic(const char *str);

//Returns the process ID of the calling process.
pid_t _sc_getpid(void);

//Returns the process ID of the parent of the calling process.
pid_t _sc_getppid(void);

//Returns the process group ID of the group of the given process.
pid_t _sc_getpgid(pid_t pid);

//Changes the group of the given process to that with the given process ID.
pid_t _sc_setpgid(pid_t pid, pid_t pgrp);

//Information about resource limits used by the kernel.
typedef struct _sc_rlimit_s
{
	long int cur;
	long int max;
} _sc_rlimit_t;

//Gets the given resource limit in the calling process.
int _sc_getrlimit(int which, _sc_rlimit_t *buf, ssize_t len);

//Sets the given resource limit in the calling process.
int _sc_setrlimit(int which, const _sc_rlimit_t *buf, ssize_t len);

//Creates a copy of the calling process.
pid_t _sc_fork(void);

//Replaces the calling process image (all its memory) with that in the given file.
//Only returns on failure - intptr_t return value is used in kernel-side mumbo jumbo.
intptr_t _sc_exec(int fd, char *const argv[], char *const envp[]);

//Opens a file descriptor referring to the given existing file.
int _sc_find(int dirfd, const char *name);

//Opens a file descriptor referring to a new file with the given name.
int _sc_make(int dirfd, const char *name, mode_t mode, int rdev);

//Access modes that the kernel recognizes about a file descriptor.
#define _SC_ACCESS_R 4
#define _SC_ACCESS_W 2
#define _SC_ACCESS_X 1

//Changes access mode / permissions of open file descriptor.
int _sc_access(int fd, int gain, int lose);

//Reads from a file.
ssize_t _sc_read(int fd, void *buf, ssize_t len);

//Writes to a file.
ssize_t _sc_write(int fd, const void *buf, ssize_t len);

//Changes file pointer in an open file.
off_t _sc_seek(int fd, off_t off, int whence);

//Changes the length of a file.
off_t _sc_trunc(int fd, off_t size);

//Removes a link to a file, deleting the file if no links remain.
int _sc_unlink(int dirfd, const char *name, int rmfd, int flags);

//Flags exposed by the kernel about file descriptors
#define _SC_FLAG_KEEPEXEC 1 //Keep the descriptor open across exec

//Sets and returns flags on the given file descriptor.
int _sc_flag(int fd, int set, int clear);

//Closes a file descriptor.
int _sc_close(int fd);

//Changes the working directory of the calling process.
int _sc_chdir(int fd);

//Duplicates a file descriptor. The new descriptor points to the same open file, sharing a file pointer (offset).
int _sc_dup(int oldfd, int min, bool overwrite);

//Format of directory entry returned from the kernel.
typedef struct _sc_dirent_s
{
	ino_t ino;
	char name[120];
} _sc_dirent_t;

//File status returned by the kernel.
typedef struct _sc_stat_s
{
	uint64_t ino;
	uint64_t dev;
	uint64_t rdev;
	off_t size;
	int mode;
} _sc_stat_t;

//Returns file status information about the file referred to by an open file descriptor.
ssize_t _sc_stat(int fd, _sc_stat_t *buf, ssize_t len);

//Device-specific IO operations recognized by the kernel
#define _SC_IOCTL_GETATTR 1
#define _SC_IOCTL_SETATTR 2
#define _SC_IOCTL_GETPGRP 3
#define _SC_IOCTL_SETPGRP 4
#define _SC_IOCTL_TTYNAME 5
#define _SC_IOCTL_ISATTY  6

//Performs device-specific IO operations on a file descriptor.
int _sc_ioctl(int fd, int operation, void *buf, ssize_t len);

//Alters the signal mask of the calling process. Returns the old mask.
int64_t _sc_sigmask(int how, int64_t mask);

//Sends a signal.
int _sc_sigsend(int idtype, int id, int sig);

//Information about why and where a signal handler was executed.
typedef struct _sc_siginfo_s
{
	//Signal number that was received
	int signum;
	
	//Address of fault, if any
	uintptr_t fault;
	
	//Sender of signal, if any
	pid_t pid;
	
	//Signal mask at the time the signal was taken.
	//When a handler is executed, the signal mask is set to ~0, so the process can safely enter/exit the handler.
	uint64_t mask;
	
	//Program counter of interrupted context
	uintptr_t pc;
	
	//Stack pointer of interrupted context
	uintptr_t sp;
	
} _sc_siginfo_t;

//Returns information about why the latest signal handler was executed.
ssize_t _sc_siginfo(_sc_siginfo_t *buf_ptr, ssize_t buf_len);

//Sleeps for the given number of nanoseconds.
int _sc_nanosleep(int64_t nsec);

//Resource usage information that the kernel tracks.
typedef struct _sc_rusage_s
{
	int utime_sec;
	int utime_usec;
	int stime_sec;
	int stime_usec;
} _sc_rusage_t;

//Returns resource usage information for the given process.
int _sc_rusage(int who, _sc_rusage_t *buf, ssize_t len);

//Information returned by kernel on return from wait.
typedef struct _sc_wait_s
{
	int status; //Wait status
	pid_t pid; //Other process
} _sc_wait_t;

//Waits for another process to change state.
ssize_t _sc_wait(int idtype, pid_t id, int options, _sc_wait_t *buf, ssize_t len);

//Changes priority of the given process; returns the new priority. If priority is negative, does not change it.
int _sc_priority(int idtype, int id, int priority);

//Returns real-time clock value, in microseconds of the GPS epoch.
int64_t _sc_getrtc(void);

//Waits until anything happens to the calling thread, or has happened since the last call returned.
void _sc_pause(void);

#endif //_SC_H
