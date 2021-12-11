//process.c
//Process tracking in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "process.h"
#include "thread.h"
#include "kassert.h"
#include "elf64.h"
#include "thread.h"
#include <errno.h>
#include <stddef.h>

//All processes on system
#define PROCESS_MAX 256
static process_t process_table[PROCESS_MAX];

void process_init(void)
{
	//Make initial process entry
	process_t *pptr = &(process_table[1]);
	m_spl_acq(&(pptr->spl));
	
	pptr->pid = 1;
	
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

void process_unlock(process_t *process)
{
	m_spl_rel(&(process->spl));
}