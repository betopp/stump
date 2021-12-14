//process.c
//Process tracking in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "process.h"
#include "thread.h"
#include "kassert.h"
#include "elf64.h"
#include "thread.h"
#include "m_tls.h"
#include <errno.h>
#include <string.h>
#include <stddef.h>

process_t process_table[PROCESS_MAX];

void process_init(void)
{
	//Make initial process entry
	process_t *pptr = &(process_table[1]);
	m_spl_acq(&(pptr->spl));
	
	pptr->pid = 1;
	pptr->state = PROCESS_STATE_ALIVE;
	
	//Open the init executable, and put a reference in our process's FD 0
	file_t *root_file = NULL;
	int root_find_err = file_find(NULL, "/", &root_file);
	KASSERT(root_find_err == 0);
	
	file_t *bin_file = NULL;
	int bin_find_err = file_find(root_file, "bin", &bin_file);
	KASSERT(bin_find_err == 0);
	
	file_t *init_file = NULL;
	int init_find_err = file_find(bin_file, "sinit", &init_file);
	KASSERT(init_find_err == 0);
	
	root_file->refs = 0;
	file_unlock(root_file);
	root_file = NULL;
	
	bin_file->refs = 0;
	file_unlock(bin_file);
	bin_file = NULL;
	
	pptr->fds[0].file = init_file;
	file_unlock(init_file);
	
	//Simulate a call to exec, to load up the process with the init ELF
	uintptr_t entry = 0;
	int elf_err = elf64_load(init_file, &(pptr->mem), &entry);
	KASSERT(elf_err == 0);
	
	//Make thread to run initial process
	thread_t *init_thread = NULL;
	int thread_err = thread_new(pptr, entry, &init_thread);
	KASSERT(thread_err == 0);
	
	thread_unlock(init_thread);
	process_unlock(pptr);
}

process_t *process_lockfree(void)
{
	for(int pp = 0; pp < PROCESS_MAX; pp++)
	{
		process_t *pptr = &(process_table[pp]);
		if(m_spl_try(&(pptr->spl)))
		{
			if(pptr->state == PROCESS_STATE_NONE)
			{
				//Found a free slot. Make sure it's got a new, valid ID corresponding to its place in the table.
				pptr->pid += PROCESS_MAX;
				if((pptr->pid <= 0) || ((pptr->pid % PROCESS_MAX) != pp))
					pptr->pid = pp;
				
				return pptr; //Still locked
			}
			
			//In use - keep looking
			m_spl_rel(&(pptr->spl));
		}
	}
	
	//No room
	return NULL;
}

process_t *process_lockpid(pid_t pid)
{
	if(pid < 0)
		return NULL;
	
	//Processes are ID'd based on their array index, so we know where this ID must be.
	process_t *pptr = &(process_table[pid % PROCESS_MAX]);
	m_spl_acq(&(pptr->spl));
	
	if((pptr->state == PROCESS_STATE_NONE) || (pptr->pid != pid))
	{
		//Wrong/no process here
		m_spl_rel(&(pptr->spl));
		return NULL;
	}
	
	//Got it, keep locked
	return pptr;
}

process_t *process_lockcur(void)
{
	thread_t *tptr = (thread_t*)(m_tls_get());	
	process_t *pptr = tptr->process;
	
	m_spl_acq(&(pptr->spl));
	return pptr;
}

void process_unlock(process_t *process)
{
	m_spl_rel(&(process->spl));
}

file_t *process_lockfd(int fd, bool allow_pwd)
{
	if(fd < -1 || fd >= PROCESS_FD_MAX)
		return NULL;
	
	if(fd < 0 && !allow_pwd)
		return NULL;
	
	process_t *pptr = process_lockcur();
	KASSERT(pptr != NULL);
	
	file_t *fptr = (fd < 0) ? pptr->pwd : pptr->fds[fd].file;
	if(fptr != NULL)
		file_lock(fptr);
	
	process_unlock(pptr);
	return fptr;
}

int process_addfd(file_t *newfile)
{
	KASSERT(newfile != NULL);
	
	process_t *pptr = process_lockcur();
	KASSERT(pptr != NULL);
	
	for(int ff = 0; ff < PROCESS_FD_MAX; ff++)
	{
		if(pptr->fds[ff].file == NULL)
		{
			pptr->fds[ff].file = newfile;
			pptr->fds[ff].flags = 0;
			process_unlock(pptr);
			return ff;
		}
	}
	
	process_unlock(pptr);
	return -EMFILE;
}

int process_strget(char *kbufptr, const char *uptr, size_t kbuflen)
{
	//Todo - validate buffer
	size_t ustrlen = strlen(uptr);
	if(ustrlen >= kbuflen)
		return -ENAMETOOLONG;
	
	memcpy(kbufptr, uptr, ustrlen);
	kbufptr[ustrlen] = '\0';
	return 0;
}

int process_memput(void *ubufptr, const void *kbufptr, size_t len)
{
	//Todo - validate buffer
	memcpy(ubufptr, kbufptr, len);
	return 0;
}
