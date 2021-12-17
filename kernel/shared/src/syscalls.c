//syscalls.c
//System call declarations on kernel-side
//Bryan E. Topp <betopp@betopp.com> 2021

#include "syscalls.h"
#include "kassert.h"
#include "argenv.h"
#include "m_panic.h"
#include "m_frame.h"
#include "process.h"
#include "thread.h"
#include "file.h"
#include "elf64.h"
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>


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
	if(pid < 0)
		return -EINVAL;
	
	process_t *pptr = (pid == 0) ? process_lockcur() : process_lockpid(pid);
	if(pptr == NULL)
		return -ESRCH;
	
	pid_t pgid = pptr->pgid;
	process_unlock(pptr);
	return pgid;
}

pid_t k_sc_setpgid(pid_t pid, pid_t pgrp)
{
	if(pid < 0)
		return -EINVAL;
	
	process_t *pptr = (pid == 0) ? process_lockcur() : process_lockpid(pid);
	if(pptr == NULL)
		return -ESRCH;
	
	pptr->pgid = pgrp;
	process_unlock(pptr);
	return pgrp;
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
	//Error returned on failure
	int err_ret = 0;
	
	//Cleaned up on failure
	thread_t *childthread = NULL;
	process_t *child = NULL;
	
	//Get current process/thread
	process_t *pptr = process_lockcur();
	KASSERT(pptr != NULL);
	
	thread_t *tptr = thread_lockcur();
	KASSERT(tptr != NULL);
		
	if(pptr->nthreads > 1)
	{
		//More than one thread active in calling process
		err_ret = -EBUSY;
		goto cleanup;
	}
	
	//Find space for new process
	child = process_lockfree();
	if(child == NULL)
	{
		//No room for more processes
		err_ret = -EAGAIN;
		goto cleanup;
	}
	
	//Find space for new thread
	childthread = thread_lockfree();
	if(childthread == NULL)
	{
		//No room for child's first thread
		err_ret = -EAGAIN;
		goto cleanup;
	}
	
	//Copy memory to new process
	KASSERT(child->mem.uspc == 0);
	KASSERT(child->mem_attempt.uspc == 0);
	int copy_err = mem_copy(&(child->mem), &(pptr->mem));
	if(copy_err < 0)
	{
		//Failed to copy memory space
		err_ret = copy_err;
		goto cleanup;
	}
	
	//Copy state of calling thread and put new thread in its process.
	m_drop_copy(&(childthread->drop), &(tptr->drop));
	childthread->state = THREAD_STATE_READY;
	childthread->process = child;
	child->nthreads = 1;
	
	//Child thread returns 0
	m_drop_retval(&(childthread->drop), 0);
	
	//Copy file descriptors and PWD
	//Note - done last because we don't have code to un-do it.
	for(int ff = 0; ff < PROCESS_FD_MAX; ff++)
	{
		if(pptr->fds[ff].file != NULL)
		{
			file_lock(pptr->fds[ff].file);
			child->fds[ff] = pptr->fds[ff];
			pptr->fds[ff].file->refs++;
			file_unlock(pptr->fds[ff].file);
		}
	}
	
	if(pptr->pwd != NULL)
	{
		file_lock(pptr->pwd);
		pptr->pwd->refs++;
		child->pwd = pptr->pwd;
		file_unlock(pptr->pwd);
	}
	
	//Success.
	child->ppid = pptr->pid;
	child->state = PROCESS_STATE_ALIVE;
	
	pid_t child_pid = child->pid;

	thread_unlock(childthread);
	thread_unlock(tptr);
	process_unlock(child);
	process_unlock(pptr);
	
	return child_pid;
	
cleanup:	
	
	if(child != NULL)
	{
		mem_clear(&(child->mem));
		mem_clear(&(child->mem_attempt));
		child->state = PROCESS_STATE_NONE;
		process_unlock(child);
	}
	
	if(childthread != NULL)
	{
		childthread->state = THREAD_STATE_NONE;
		thread_unlock(childthread);
	}
	
	process_unlock(pptr);
	thread_unlock(tptr);
	
	KASSERT(err_ret < 0);
	return err_ret;
}

intptr_t k_sc_exec(int fd, char *const argv[], char *const envp[])
{
	//Stuff for cleaning up safely
	process_t *pptr = NULL;
	file_t *elf_file = NULL;
	int err_ret = 0;
	
	//Get the ELF file and try to load it into this process's spare memory space.
	elf_file = process_lockfd(fd, false);
	if(elf_file == NULL)
	{
		err_ret = -EBADF;
		goto cleanup;
	}
	
	pptr = process_lockcur();
	KASSERT(pptr->mem_attempt.uspc == 0);
	if(pptr->nthreads > 1)
	{
		//Can't exec while other threads are running
		err_ret = -EBUSY;
		goto cleanup;
	}
	
	uintptr_t elf_entry = 0;
	int elf_err = elf64_load(elf_file, &(pptr->mem_attempt), &elf_entry);
	if(elf_err < 0)
	{
		//Failed to load the new process image.
		err_ret = elf_err;
		goto cleanup;
	}
	
	//Try to copy the arg/env buffer into the new memory space	
	uintptr_t argenv_addr = 0;
	int argenv_err = argenv_load(&(pptr->mem_attempt), argv, envp, &argenv_addr);
	if(argenv_err < 0)
	{
		//Failed to copy argv/envp
		err_ret = argenv_err;
		goto cleanup;
	}
	
	//Set up the new memory space successfully. Switch over and ditch the old memory.
	m_uspc_activate(pptr->mem_attempt.uspc);
	mem_clear(&(pptr->mem));
	pptr->mem = pptr->mem_attempt;
	memset(&(pptr->mem_attempt), 0, sizeof(pptr->mem_attempt));
	
	//Done setting up the process...
	process_unlock(pptr);
	pptr = NULL;
	
	//Reset the user context for the calling thread, to start at the new entry point
	thread_t *tptr = thread_lockcur();
	m_drop_reset(&(tptr->drop), elf_entry);
	m_drop_retval(&(tptr->drop), argenv_addr);
	thread_unlock(tptr);
	
	//Successo
	return argenv_addr;
	
cleanup:
	if(pptr != NULL)
	{
		mem_clear(&(pptr->mem_attempt));		
		process_unlock(pptr);
		pptr = NULL;
	}
	
	if(elf_file != NULL)
	{
		file_unlock(elf_file);
		elf_file = NULL;
	}
		
	KASSERT(err_ret < 0);
	return err_ret;
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

int k_sc_mem_free(uintptr_t addr, ssize_t size)
{
	(void)addr;
	(void)size;
	KASSERT(0);
	return -ENOSYS;
}

ssize_t k_sc_wait(int idtype, pid_t id, int options, _sc_wait_t *buf, ssize_t len)
{
	if(id < 0)
		return -EINVAL;
	
	if(len != sizeof(_sc_wait_t)) //Todo - support versioning here if _sc_wait_t changes
		return -EINVAL;
	
	//Figure out our ID - whose children we're looking for
	process_t *pptr = process_lockcur();
	pid_t ourpid = pptr->pid;
	
	//Handle special-case - zero PID/PGID means "same group as the caller"
	if( (idtype == P_PID || idtype == P_PGID) && (id == 0) )
	{
		idtype = P_PGID;
		id = pptr->pgid;
	}
	
	process_unlock(pptr);
	pptr = NULL;
	
	//Run through the process table once and see what's up.
	//Caller should use _sc_pause and try again if we tell them so.
	//Anyone currently in the process of posting status will poke their parent after unlocking.
	bool any_match = false;
	for(int pp = 0; pp < PROCESS_MAX; pp++)
	{
		process_t *otherproc = &(process_table[pp]);
		m_spl_acq(&(otherproc->spl));
		
		if((otherproc->state == PROCESS_STATE_NONE) || (otherproc->ppid != ourpid))
		{
			//Not one of our children
			m_spl_rel(&(otherproc->spl));
			continue;
		}
		
		//Found a child of ours. Does it match the ID we want?
		bool matches_id = false;
		switch(idtype)
		{
			case P_PID:
				matches_id = (otherproc->pid == id);
				break;
			case P_PGID:
				matches_id = (otherproc->pgid == id);
				break;
			case P_ALL:
				matches_id = true;
				break;
			default:
				m_spl_rel(&(otherproc->spl));
				return -EINVAL;
		}
		
		if(!matches_id)
		{
			//It's one of our children, but doesn't match the ID we asked for.
			m_spl_rel(&(otherproc->spl));
			continue;
		}
		
		//There's at least one process matching the query.
		//This means we won't return -ECHILD.
		any_match = true;
		
		//See if the process has the type of status info we're looking for.
		bool has_status = false;		
		if( (options & WEXITED) && (WIFEXITED(otherproc->wstatus ) || WIFSIGNALED(otherproc->wstatus)) )
			has_status = true;
		if( (options & WSTOPPED) && WIFSTOPPED(otherproc->wstatus) )
			has_status = true;
		if( (options & WCONTINUED) && WIFCONTINUED(otherproc->wstatus) )
			has_status = true;
		
		if(!has_status)
		{
			//Doesn't have the kind of status information we're looking for.
			m_spl_rel(&(otherproc->spl));
			continue;
		}
		
		//Alright, found what we're waiting for.
		_sc_wait_t output = {0};
		output.status = otherproc->wstatus;
		output.pid = otherproc->pid;
		
		//Clear their status unless we were asked not to
		if(!(options & WNOWAIT))
		{			
			otherproc->wstatus = 0;
			
			if(otherproc->state == PROCESS_STATE_DEAD)
			{
				//Just waited on an otherwise dead process. 
				//It should be mostly cleaned-up already - finish it off.
				KASSERT(otherproc->mem.uspc == 0);
				KASSERT(otherproc->mem_attempt.uspc == 0);
				KASSERT(otherproc->nthreads == 0);
				for(int ff = 0; ff < PROCESS_FD_MAX; ff++)
				{
					KASSERT(otherproc->fds[ff].file == NULL);
				}
				KASSERT(otherproc->pwd == NULL);
				
				otherproc->state = PROCESS_STATE_NONE;
				otherproc->ppid = 0;
			}
		}
			
		//Done. Return the data.
		m_spl_rel(&(otherproc->spl));
		
		int copy_err = process_memput(buf, &output, len);
		if(copy_err < 0)
			return copy_err;
		
		return len;
	}
	
	//Didn't find anybody with status we wanted.
	if(!any_match)
	{
		//No children match this query - no status would ever be forthcoming.
		return -ECHILD;
	}
	
	//There are matching children, but none has status right now.
	return 0; 
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
	thread_t *tptr = thread_lockcur();
	if(tptr->unpauses > tptr->unpauses_past)
	{
		//Somebody has unpaused us since we last returned. Return immediately.
		tptr->unpauses_past = tptr->unpauses;
		thread_unlock(tptr);
		return;
	}
	
	//Nobody's unpaused us since we last returned. This thread blocks.
	tptr->state = THREAD_STATE_WAIT;
	thread_unlock(tptr);
	return;
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

