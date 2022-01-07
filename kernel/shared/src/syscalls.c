//syscalls.c
//System call declarations on kernel-side
//Bryan E. Topp <betopp@betopp.com> 2021

#include "syscalls.h"
#include "kassert.h"
#include "argenv.h"
#include "kpage.h"
#include "m_panic.h"
#include "m_frame.h"
#include "process.h"
#include "thread.h"
#include "file.h"
#include "pipe.h"
#include "elf.h"
#include "m_time.h"
#include "con.h"
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>

void k_sc_none(void)
{
	
}

void k_sc_exit(int exitcode, int signal)
{
	//Mark the process as wanting to exit, and store the exit status
	process_t *pptr = process_lockcur();
	if(pptr->state == PROCESS_STATE_DETHREAD)
	{
		//Some other thread already made our process quit.
		process_unlock(pptr);
		return;
	}
	
	KASSERT(pptr->state == PROCESS_STATE_ALIVE);
	pptr->state = PROCESS_STATE_DETHREAD;
	if(signal)
		pptr->wstatus = _WIFSIGNALED_FLAG | ((signal << _WTERMSIG_SHIFT) & _WTERMSIG_MASK);
	else
		pptr->wstatus = _WIFEXITED_FLAG | (exitcode & 0xFF);
	
	process_unlock(pptr);
	
	//When we try to return from the system call, we'll see that this thread belongs to a dethreading process.
	//The thread will be removed and cleaned-up, and if it's the last one, so will the process.
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
	int thread_err = thread_new(child, 0, &childthread);
	if(thread_err < 0)
	{
		//No room for child's first thread
		err_ret = thread_err;
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
	
	//Copy state of calling thread and put new thread in its process, except the child thread returns 0.
	m_drop_copy(&(childthread->drop), &(tptr->drop));
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
	
	if(!(elf_file->access & _SC_ACCESS_X))
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
	int elf_err = elf_load(elf_file, &(pptr->mem_attempt), &elf_entry);
	if(elf_err < 0)
	{
		//Failed to load the new process image.
		err_ret = elf_err;
		goto cleanup;
	}
	
	//Done with ELF now
	file_unlock(elf_file);
	elf_file = NULL;
	
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
	
	//Drop any file descriptors not flagged keep-on-exec
	for(int ff = 0; ff < PROCESS_FD_MAX; ff++)
	{
		if(pptr->fds[ff].file == NULL)
			continue;
	
		if(pptr->fds[ff].flags & _SC_FLAG_KEEPEXEC)
			continue;
		
		file_lock(pptr->fds[ff].file);
		pptr->fds[ff].file->refs--;
		file_unlock(pptr->fds[ff].file);
		pptr->fds[ff].file = NULL;
		pptr->fds[ff].flags = 0;
	}
	
	//Done setting up the process...
	process_unlock(pptr);
	pptr = NULL;
	
	//Reset the user context for the calling thread, to start at the new entry point
	thread_t *tptr = thread_lockcur();
	tptr->sigmask = 0x7FFFFFFFFFFFFFFFul;
	tptr->sigpend = 0;
	tptr->sigpc = 0;
	tptr->sigsp = 0;
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
	if((gain < 0) || (lose < 0))
		return -EINVAL;
	
	file_t *fptr = process_lockfd(fd, false);
	if(fptr == NULL)
		return -EBADF;
	
	int oldaccess = fptr->access;
	fptr->access |= gain;
	fptr->access &= ~lose;
	
	//If this is a pipe, we need to change its reference-count of readers/writers
	if(S_ISFIFO(fptr->mode))
	{
		int pipe_id = (fptr->special > 0) ? fptr->special : -fptr->special;
		pipe_dir_t pipe_dir = (fptr->special > 0) ? PIPE_DIR_FORWARD : PIPE_DIR_REVERSE;
		
		pipe_t *pipe = pipe_lockid(pipe_id);
		KASSERT(pipe != NULL);
		
		if(oldaccess & _SC_ACCESS_R)
			pipe->dirs[pipe_dir].refs_r--;
		if(oldaccess & _SC_ACCESS_W)
			pipe->dirs[pipe_dir].refs_w--;
		
		if(fptr->access & _SC_ACCESS_R)
			pipe->dirs[pipe_dir].refs_r++;
		if(fptr->access & _SC_ACCESS_W)
			pipe->dirs[pipe_dir].refs_w++;
		
		pipe_unlock(pipe);
	}
	
	int retval = fptr->access;
	KASSERT(retval >= 0);
	file_unlock(fptr);
	return retval;
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
	//Process can specify rmfd==-1 for "remove any file with this name".
	//They can also specify a file descriptor in rmfd - then, only that exact file may be unlinked.
	if(rmfd < -1)
		return -EINVAL;
	
	file_t *dirptr = process_lockfd(dirfd, true);
	if(dirptr == NULL)
		return -EBADF;
	
	file_t *rmptr = NULL;
	if(rmfd != -1)
	{
		rmptr = process_lockfd(rmfd, false);
		if(rmptr == NULL)
		{
			file_unlock(dirptr);
			return -EBADF;
		}
	}
	
	int result = file_unlink(dirptr, name, rmptr, flags);
	file_unlock(dirptr);
	if(rmptr != NULL)
		file_unlock(rmptr);
	
	return result;
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
	file_t *fptr = process_lockfd(fd, false);
	if(fptr == NULL)
		return -EBADF;
	
	int result = file_ioctl(fptr, operation, buf, len);
	file_unlock(fptr);
	return result;
}

int k_sc_nanosleep(int64_t nsec)
{
	(void)nsec;
	KASSERT(0);
	return -ENOSYS;
}

int k_sc_rusage(int who, _sc_rusage_t *buf, ssize_t len)
{
	if(who != RUSAGE_THREAD && who != RUSAGE_SELF && who != RUSAGE_CHILDREN)
		return -EINVAL;
	
	if(len < 1)
		return -EINVAL;
	
	if(len > (ssize_t)sizeof(_sc_rusage_t))
		len = sizeof(_sc_rusage_t);
	
	_sc_rusage_t rusage = {0};
	//Todo - fill in
	
	int copy_err = process_memput(buf, &rusage, len);
	if(copy_err < 0)
		return copy_err;
	
	return len;
}

intptr_t k_sc_mem_avail(intptr_t around, ssize_t size)
{
	process_t *pptr = process_lockcur();
	intptr_t retval = mem_avail(&(pptr->mem), around, size);
	process_unlock(pptr);
	return retval;
}

int k_sc_mem_anon(uintptr_t addr, ssize_t size, int access)
{
	process_t *pptr = process_lockcur();
	
	int retval = mem_add(&(pptr->mem), addr, size, access);
	if(retval >= 0)
	{
		size_t pagesize = m_frame_size();
		size_t clear_size = size;
		if(clear_size % pagesize)
			clear_size += pagesize - (clear_size % pagesize);
		
		memset((void*)addr, 0, clear_size);
	}
	
	process_unlock(pptr);
	return retval;
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
	//temp
	return m_time_tsc() / 4096;
}

void k_sc_pause(void)
{
	thread_t *tptr = thread_lockcur();
	
	//Unpauses gets incremented atomically without holding the thread's lock.
	//So, capture it while we work with it.
	m_atomic_t unpauses_now = (volatile m_atomic_t)(tptr->unpauses);
	
	//Each time we call pause, we require at least one corresponding unpause.
	//But if we've got an excess of unpauses, consume them all at once.
	tptr->unpauses_req++;
	if(tptr->unpauses_req < unpauses_now)
		tptr->unpauses_req = unpauses_now;
		
	thread_unlock(tptr);
	return;
}

int k_sc_con_init(const _sc_con_init_t *buf_ptr, ssize_t buf_len)
{
	//We're initializing the console, so the calling thread will be the one to get unpaused by its events.
	thread_t *tptr = thread_lockcur();
	id_t contid = tptr->tid;
	thread_unlock(tptr);
	tptr = NULL;
	
	//Copy parameters from user
	_sc_con_init_t parms = {0};
	if(buf_len > (ssize_t)sizeof(parms))
		buf_len = sizeof(parms);
	
	int parm_err = process_memget(&parms, buf_ptr, buf_len);
	if(parm_err < 0)
		return parm_err;
	
	//Validate framebuffer
	if(parms.fb_width < 1 || parms.fb_width > 4096)
		return -EINVAL;
	if(parms.fb_height < 1 || parms.fb_height > 4096)
		return -EINVAL;
	if(parms.fb_stride < (size_t)parms.fb_width || parms.fb_stride > 1048576)
		return -EINVAL;
	if(parms.flags != 0)
		return -EINVAL;
	
	//Try to allocate a kernel buffer for the console framebuffer
	size_t fblen = parms.fb_height * parms.fb_stride;
	void *fbptr = kpage_alloc(fblen);
	if(fbptr == NULL)
		return -ENOMEM;
	
	//Put it in the current process, freeing any old buffer
	process_t *pptr = process_lockcur();
	if(pptr->fb.bufptr != NULL)
		kpage_free(pptr->fb.bufptr, pptr->fb.buflen);
	
	pptr->fb.bufptr = fbptr;
	pptr->fb.buflen = fblen;
	pptr->fb.width = parms.fb_width;
	pptr->fb.height = parms.fb_height;
	pptr->fb.stride = parms.fb_stride;
	
	pptr->contid = contid;
	
	if(pptr->hascon)
		con_settid(contid);
	
	process_unlock(pptr);
	pptr = NULL;
	
	//Success
	return 0;
}

int k_sc_con_flip(const void *fb_ptr, int flags)
{
	//Flags eventually will indicate scaling/vsync behavior and stuff
	if(flags != 0)
		return -EINVAL;
	
	//Copy into the kernel-side backbuffer for this process.
	process_t *pptr = process_lockcur();
	if(pptr->fb.bufptr == NULL)
	{
		//This process hasn't set up their console
		process_unlock(pptr);
		return -ENXIO;
	}
	
	int mem_err = process_memget(pptr->fb.bufptr, fb_ptr, pptr->fb.buflen);
	if(mem_err < 0)
	{
		//Failed to copy the user's image into our kernel-side buffer.
		process_unlock(pptr);
		return mem_err;
	}
	
	//If this process holds the console, immediately display the image
	//Todo - eh probly triple buffer or something
	if(pptr->hascon)
		fb_paint(&(pptr->fb));
	
	process_unlock(pptr);
	return 0;
}

ssize_t k_sc_con_input(_sc_con_input_t *buf_ptr, ssize_t each_bytes, ssize_t buf_bytes)
{
	//Todo - can version this based on length of input event structure
	if(each_bytes != sizeof(_sc_con_input_t))
		return -EINVAL;
	
	process_t *pptr = process_lockcur();
	
	//Have to hold the console to get input
	if(!pptr->hascon)
	{
		//Process doesn't have the console. Return no input.
		process_unlock(pptr);
		return 0;
	}
	
	ssize_t retval = con_input(buf_ptr, buf_bytes);
	process_unlock(pptr);
	return retval;
}

int k_sc_con_pass(pid_t next)
{
	process_t *pptr = process_lockcur();
	
	//Permit PID1 to steal the console at user's direction
	if(pptr->pid == 1)
	{
		if(con_steal_check())
		{
			//Take console away from previous holder
			for(int pp = 0; pp < PROCESS_MAX; pp++)
			{
				process_t *from_pptr = &(process_table[pp]);
				if(from_pptr == pptr)
					continue;
				
				m_spl_acq(&(from_pptr->spl));
				if(from_pptr->hascon)
					from_pptr->hascon = false;
				
				m_spl_rel(&(from_pptr->spl));
			}
			
			//We own it now
			pptr->hascon = true;
			con_settid(1);
		}
	}
	
	if(!pptr->hascon)
	{
		//Don't have the console right now, can't pass it
		process_unlock(pptr);
		return -EBUSY;
	}
	
	if(pptr->pid == next)
	{
		//Passing to ourself...?
		process_unlock(pptr);
		return 0;
	}
		
	process_t *newpptr = process_lockpid(next);
	if(newpptr == NULL)
	{
		//No such target
		process_unlock(pptr);
		return -ESRCH;
	}
	
	if(newpptr->state != PROCESS_STATE_ALIVE)
	{
		process_unlock(newpptr);
		process_unlock(pptr);
		return -ESRCH;
	}
	
	pptr->hascon = false;
	newpptr->hascon = true;
	id_t notify_tid = newpptr->contid;
	
	process_unlock(pptr);
	process_unlock(newpptr);
	
	con_settid(notify_tid);
	
	return 0;
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

