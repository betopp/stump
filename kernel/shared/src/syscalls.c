//syscalls.c
//System call declarations on kernel-side
//Bryan E. Topp <betopp@betopp.com> 2021

#include "syscalls.h"
#include "kassert.h"
#include "m_panic.h"
#include "process.h"
#include "file.h"
#include <errno.h>
#include <string.h>
#include <sys/stat.h>


void k_sc_none(void)
{
	
}

void k_sc_exit(int exitcode, int signal)
{
	(void)exitcode;
	(void)signal;
	KASSERT(0);
}

int k_sc_panic(const char *str)
{
	m_panic(str);
}

pid_t k_sc_getpid(void)
{
	process_t *pptr = process_lockcur();
	pid_t retval = pptr->pid;
	process_unlock(pptr);
	return retval;
}

pid_t k_sc_getppid(void)
{
	process_t *pptr = process_lockcur();
	pid_t retval = pptr->ppid;
	process_unlock(pptr);
	return retval;
}

pid_t k_sc_getpgid(pid_t pid)
{
	(void)pid;
	KASSERT(0);
	return -ENOSYS;
}

pid_t k_sc_setpgid(pid_t pid, pid_t pgrp)
{
	(void)pid;
	(void)pgrp;
	KASSERT(0);
	return -ENOSYS;
}

int k_sc_getrlimit(int which, _sc_rlimit_t *buf, ssize_t len)
{
	(void)which;
	(void)buf;
	(void)len;
	KASSERT(0);
	return -ENOSYS;
}

int k_sc_setrlimit(int which, const _sc_rlimit_t *buf, ssize_t len)
{
	(void)which;
	(void)buf;
	(void)len;
	KASSERT(0);
	return -ENOSYS;
}

pid_t k_sc_fork(void)
{
	KASSERT(0);
	return -ENOSYS;
}

int k_sc_exec(int fd, char *const argv[], char *const envp[])
{
	(void)fd;
	(void)argv;
	(void)envp;
	KASSERT(0);
	return -ENOSYS;
}

int k_sc_find(int dirfd, const char *name)
{
	//Get name string into kernel safely
	char namebuf[128];
	int name_err = process_strget(namebuf, name, sizeof(namebuf));
	if(name_err < 0)
		return name_err;
	
	//Get directory reference
	file_t *dptr = process_lockfd(dirfd, true);
	if(dptr == NULL && strcmp(name, "/") != 0) //Allow bad directory reference if we're looking for "/"
		return -EBADF;
	
	//Look up in directory
	file_t *found = NULL;
	int find_result = file_find(dptr, namebuf, &found);
	
	if(dptr != NULL)
		file_unlock(dptr);
	
	if(find_result < 0)
		return find_result;

	//Try to insert into FDs for this process
	int newfd = process_addfd(found);
	if(newfd < 0)
		found->refs = 0;
	
	file_unlock(found);
	return newfd;
}

int k_sc_make(int dirfd, const char *name, mode_t mode, int rdev)
{
	//Get name string into kernel safely
	char namebuf[128];
	int name_err = process_strget(namebuf, name, sizeof(namebuf));
	if(name_err < 0)
		return name_err;
	
	//Get directory reference
	file_t *dptr = process_lockfd(dirfd, true);
	if(dptr == NULL)
		return -EBADF;
	
	//Try to make the file
	file_t *made = NULL;
	int make_result = file_make(dptr, namebuf, mode, rdev, &made);
	file_unlock(dptr);
	
	if(make_result < 0)
		return make_result;

	//Try to insert into FDs for this process
	int newfd = process_addfd(made);
	if(newfd < 0)
		made->refs = 0;
	
	file_unlock(made);
	return newfd;
}

int k_sc_access(int fd, int gain, int lose)
{
	(void)fd;
	(void)gain;
	(void)lose;
	KASSERT(0);
	return -ENOSYS;
}

ssize_t k_sc_read(int fd, void *buf, ssize_t len)
{
	file_t *fptr = process_lockfd(fd, false);
	if(fptr == NULL)
		return -EBADF;
	
	ssize_t result = file_read(fptr, buf, len); //Todo - validate buffer
	file_unlock(fptr);
	return result;
}

ssize_t k_sc_write(int fd, const void *buf, ssize_t len)
{
	file_t *fptr = process_lockfd(fd, false);
	if(fptr == NULL)
		return -EBADF;
	
	ssize_t result = file_write(fptr, buf, len); //Todo - validate buffer
	file_unlock(fptr);
	return result;
}

off_t k_sc_seek(int fd, off_t off, int whence)
{
	file_t *fptr = process_lockfd(fd, false);
	if(fptr == NULL)
		return -EBADF;
	
	off_t result = file_seek(fptr, off, whence);
	file_unlock(fptr);
	return result;
}

off_t k_sc_trunc(int fd, off_t size)
{
	file_t *fptr = process_lockfd(fd, false);
	if(fptr == NULL)
		return -EBADF;
	
	off_t result = file_trunc(fptr, size);
	file_unlock(fptr);
	return result;
}

int k_sc_unlink(int dirfd, const char *name, int rmfd, int flags)
{
	(void)dirfd;
	(void)name;
	(void)rmfd;
	(void)flags;
	KASSERT(0);
	return -ENOSYS;
}

int k_sc_flag(int fd, int set, int clear)
{
	if(set < 0 || clear < 0)
		return -EINVAL;
	
	if(fd < 0 || fd >= PROCESS_FD_MAX)
		return -EBADF;
	
	process_t *pptr = process_lockcur();
	if(pptr->fds[fd].file == NULL)
	{
		process_unlock(pptr);
		return -EBADF;
	}
	
	pptr->fds[fd].flags |= set;
	pptr->fds[fd].flags &= ~(clear);
	int retval = pptr->fds[fd].flags;
	
	process_unlock(pptr);
	return retval;
}

int k_sc_close(int fd)
{
	if(fd < 0 || fd >= PROCESS_FD_MAX)
		return -EBADF;
	
	process_t *pptr = process_lockcur();
	if(pptr->fds[fd].file == NULL)
	{
		process_unlock(pptr);
		return -EBADF;
	}
	
	file_t *fptr = pptr->fds[fd].file;
	file_lock(fptr);
	pptr->fds[fd].file = NULL;
	fptr->refs--;
	file_unlock(fptr);
	process_unlock(pptr);
	
	return 0;	
}

int k_sc_chdir(int fd)
{
	if(fd < 0 || fd >= PROCESS_FD_MAX)
		return -EBADF;
	
	process_t *pptr = process_lockcur();
	if(pptr->fds[fd].file == NULL)
	{
		process_unlock(pptr);
		return -EBADF;
	}
	
	if(pptr->pwd != NULL)
	{
		file_lock(pptr->pwd);
		pptr->pwd->refs--;
		file_unlock(pptr->pwd);
		pptr->pwd = NULL;
	}
	
	file_lock(pptr->fds[fd].file);
	pptr->pwd = pptr->fds[fd].file;
	pptr->pwd->refs++;
	file_unlock(pptr->fds[fd].file);
	
	process_unlock(pptr);
	return 0;
}

int k_sc_dup(int oldfd, int min, bool overwrite)
{
	if(oldfd < 0 || oldfd >= PROCESS_FD_MAX)
		return -EBADF;
	
	if(min < 0 || min >= PROCESS_FD_MAX)
		return -EBADF;
	
	if( (oldfd == min) && overwrite )
		return oldfd;
	
	process_t *pptr = process_lockcur();
	if(pptr->fds[oldfd].file == NULL)
	{
		process_unlock(pptr);
		return -EBADF;
	}
	
	for(int ff = min; ff < PROCESS_FD_MAX; ff++)
	{
		if(overwrite || (pptr->fds[ff].file == NULL))
		{
			if(pptr->fds[ff].file != NULL)
			{
				file_lock(pptr->fds[ff].file);
				pptr->fds[ff].file->refs--;
				file_unlock(pptr->fds[ff].file);
				pptr->fds[ff].file = NULL;
			}
			
			file_lock(pptr->fds[oldfd].file);
			pptr->fds[ff].file = pptr->fds[oldfd].file;
			pptr->fds[oldfd].file->refs++;
			file_unlock(pptr->fds[oldfd].file);
			
			process_unlock(pptr);
			return ff;
		}
	}
	
	//Didn't have room
	process_unlock(pptr);
	return -EMFILE;
}

ssize_t k_sc_stat(int fd, _sc_stat_t *buf, ssize_t len)
{
	file_t *fptr = process_lockfd(fd, true);
	if(fptr == NULL)
		return -EBADF;
	
	//Kinda silly to ping-pong like this, but whatever.
	//There is some value in having an explicit "this is what the syscall expects" definition.
	struct stat st;
	int stat_err = file_stat(fptr, &st);
	if(stat_err < 0)
	{
		file_unlock(fptr);
		return stat_err;
	}
	
	_sc_stat_t kbuf;
	kbuf.ino = st.st_ino;
	kbuf.dev = st.st_dev;
	kbuf.rdev = st.st_rdev;
	kbuf.size = st.st_size;
	kbuf.mode = st.st_mode;
	
	ssize_t to_copy = (len < (ssize_t)sizeof(kbuf)) ? len : (ssize_t)sizeof(kbuf);
	int buf_err = process_memput(buf, &kbuf, to_copy);
	if(buf_err < 0)
	{
		file_unlock(fptr);
		return buf_err;
	}
	
	file_unlock(fptr);
	return to_copy;
}

int k_sc_ioctl(int fd, int operation, void *buf, ssize_t len)
{
	(void)fd;
	(void)operation;
	(void)buf;
	(void)len;
	KASSERT(0);
	return -ENOSYS;
}

int64_t k_sc_sigmask(int how, int64_t mask)
{
	(void)how;
	(void)mask;
	KASSERT(0);
	return -ENOSYS;
}

int k_sc_sigsuspend(int64_t mask)
{
	(void)mask;
	KASSERT(0);
	return -ENOSYS;
}

int k_sc_sigsend(int idtype, int id, int sig)
{
	(void)idtype;
	(void)id;
	(void)sig;
	KASSERT(0);
	return -ENOSYS;
}

ssize_t k_sc_siginfo(_sc_siginfo_t *buf_ptr, ssize_t buf_len)
{
	(void)buf_ptr;
	(void)buf_len;
	KASSERT(0);
	return -ENOSYS;
}

int k_sc_nanosleep(int64_t nsec)
{
	(void)nsec;
	KASSERT(0);
	return -ENOSYS;
}

int k_sc_rusage(int who, _sc_rusage_t *buf, ssize_t len)
{
	(void)who;
	(void)buf;
	(void)len;
	KASSERT(0);
	return -ENOSYS;
}

intptr_t k_sc_mem_avail(intptr_t around, ssize_t size)
{
	(void)around;
	(void)size;
	KASSERT(0);
	return -ENOSYS;
}

int k_sc_mem_anon(uintptr_t addr, ssize_t size, int access)
{
	(void)addr;
	(void)size;
	(void)access;
	KASSERT(0);
	return -ENOSYS;
}

ssize_t k_sc_wait(int idtype, pid_t id, int options, _sc_wait_t *buf, ssize_t len)
{
	(void)idtype;
	(void)id;
	(void)options;
	(void)buf;
	(void)len;
	KASSERT(0);
	return -ENOSYS;
}

int k_sc_priority(int idtype, int id, int priority)
{
	(void)idtype;
	(void)id;
	(void)priority;
	KASSERT(0);
	return -ENOSYS;
}

int64_t k_sc_getrtc(void)
{
	KASSERT(0);
	return -ENOSYS;
}

void k_sc_pause(void)
{
	
}


uintptr_t syscalls_handle(uintptr_t num, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, uintptr_t p5)
{
	//Use macro-trick to make a switch statement by call-number.
	switch(num)
	{
		#define SYSCALL0R(num, rt,  name) \
			case num: return (uintptr_t) k##name();
		#define SYSCALL1R(num, rt,  name, p1t) \
			case num: return (uintptr_t) k##name((p1t)p1);
		#define SYSCALL2R(num, rt,  name, p1t, p2t) \
			case num: return (uintptr_t) k##name((p1t)p1, (p2t)p2);
		#define SYSCALL3R(num, rt,  name, p1t, p2t, p3t) \
			case num: return (uintptr_t) k##name((p1t)p1, (p2t)p2, (p3t)p3);
		#define SYSCALL4R(num, rt,  name, p1t, p2t, p3t, p4t) \
			case num: return (uintptr_t) k##name((p1t)p1, (p2t)p2, (p3t)p3, (p4t)p4);
		#define SYSCALL5R(num, rt,  name, p1t, p2t, p3t, p4t, p5t) \
			case num: return (uintptr_t) k##name((p1t)p1, (p2t)p2, (p3t)p3, (p4t)p4, (p5t)p5);

		#define SYSCALL0V(num, rt,  name) \
			case num: k##name(); return 0;
		#define SYSCALL1V(num, rt,  name, p1t) \
			case num: k##name((p1t)p1); return 0;
		#define SYSCALL2V(num, rt,  name, p1t, p2t) \
			case num: k##name((p1t)p1, (p2t)p2); return 0;
		#define SYSCALL3V(num, rt,  name, p1t, p2t, p3t) \
			case num: k##name((p1t)p1, (p2t)p2, (p3t)p3); return 0;
		#define SYSCALL4V(num, rt,  name, p1t, p2t, p3t, p4t) \
			case num: k##name((p1t)p1, (p2t)p2, (p3t)p3, (p4t)p4); return 0;
		#define SYSCALL5V(num, rt,  name, p1t, p2t, p3t, p4t, p5t) \
			case num: k##name((p1t)p1, (p2t)p2, (p3t)p3, (p4t)p4, (p5t)p5); return 0;

		#define SYSCALL0N(num, rt,  name) \
			case num: k##name(); return 0;
		#define SYSCALL1N(num, rt,  name, p1t) \
			case num: k##name((p1t)p1); return 0;
		#define SYSCALL2N(num, rt,  name, p1t, p2t) \
			case num: k##name((p1t)p1, (p2t)p2); return 0;
		#define SYSCALL3N(num, rt,  name, p1t, p2t, p3t) \
			case num: k##name((p1t)p1, (p2t)p2, (p3t)p3); return 0;
		#define SYSCALL4N(num, rt,  name, p1t, p2t, p3t, p4t) \
			case num: k##name((p1t)p1, (p2t)p2, (p3t)p3, (p4t)p4); return 0;
		#define SYSCALL5N(num, rt,  name, p1t, p2t, p3t, p4t, p5t) \
			case num: k##name((p1t)p1, (p2t)p2, (p3t)p3, (p4t)p4, (p5t)p5); return 0;
		
		#include "systable.h"
		
		default:
			return -ENOSYS;
	}
}

