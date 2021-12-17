//argenv.c
//Argument/environment loading for new processes
//Bryan E. Topp <betopp@betopp.com> 2021

#include "argenv.h"
#include "kpage.h"
#include "kassert.h"
#include "process.h"
#include "m_frame.h"
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>

int argenv_load(mem_t *mem, char * const * argv, char * const * envp, uintptr_t *loaded_out)
{
	//Todo - need to verify all this crap from the calling process.
	
	//Count length of strings.
	size_t argv_count = 0;
	size_t argv_strlen = 0;
	while(1)
	{
		if(argv[argv_count] == NULL)
			break;
		
		argv_strlen += strlen(argv[argv_count]) + 1;
		argv_count++;
	}
	
	size_t envp_count = 0;
	size_t envp_strlen = 0;
	while(1)
	{
		if(envp[envp_count] == NULL)
			break;
		
		envp_strlen += strlen(envp[envp_count]) + 1;
		envp_count++;
	}
	
	//We need the first two pointers to the pointer-arrays.
	//Then we need the pointer arrays, each NULL-terminated.
	//Then we need the strings, each NUL-terminated.
	size_t argenv_size = (2 * sizeof(char**)) + ((envp_count + 1 + argv_count + 1) * sizeof(char*)) + argv_strlen + envp_strlen;
	if(argenv_size > 65536)
	{
		//Size of arguments and environment too big.
		return -E2BIG;
	}
	
	//Get space in kernel-space, so we can copy across the two user-spaces
	char *argenv_kbuf = kpage_alloc(argenv_size);
	if(argenv_kbuf == NULL)
	{
		//Not enough memory for kernel-side copy
		return -ENOMEM;
	}
	memset(argenv_kbuf, 0, argenv_size);
	
	//Find space and add a segment to the target memory-space
	size_t argenv_segsize = argenv_size;
	size_t pagesize = m_frame_size();
	if(argenv_segsize % pagesize != 0)
		argenv_segsize += pagesize - (argenv_segsize % pagesize);
	
	intptr_t argenv_addr = mem_avail(mem, ~0ull, argenv_segsize);
	if(argenv_addr < 0)
	{
		//No free address space
		kpage_free(argenv_kbuf, argenv_size);
		return argenv_addr;
	}
	
	int add_err = mem_add(mem, argenv_addr, argenv_segsize, M_USPC_PROT_R | M_USPC_PROT_W);
	if(add_err < 0)
	{
		//Probably ran out of RAM - couldn't make a memory segment to hold the args/env.
		kpage_free(argenv_kbuf, argenv_size);
		return add_err;
	}
	
	//Copy across - into kernel-space, then out into the new userspace
	
	//Beginning of arg/env blob is pointer to arg-strings array, then pointer to env-strings array.
	//arg-strings array starts right after. env-strings array starts after all arg-string pointers (and NULL).
	//We need to keep track of offset-within-segment, because this is at different locations now (kernel) and finally (user).
	ptrdiff_t arg_ptrs_off = 2 * sizeof(char**);
	ptrdiff_t env_ptrs_off = arg_ptrs_off + ((argv_count+1) * sizeof(char*));
	ptrdiff_t str_data_off = env_ptrs_off + ((envp_count+1) * sizeof(char*));
	
	char ***array_ptrs = (char***)argenv_kbuf;
	array_ptrs[0] = (char**)(argenv_addr + arg_ptrs_off);
	array_ptrs[1] = (char**)(argenv_addr + env_ptrs_off);
	
	char **arg_ptrs = (char**)(argenv_kbuf + arg_ptrs_off);
	for(size_t aa = 0; aa < argv_count; aa++)
	{
		size_t aalen = strlen(argv[aa]);
		arg_ptrs[aa] = (char*)(argenv_addr + str_data_off);
		memcpy(argenv_kbuf + str_data_off, argv[aa], aalen);
		str_data_off += aalen + 1;
	}
	
	char **env_ptrs = (char**)(argenv_kbuf + env_ptrs_off);
	for(size_t ee = 0; ee < envp_count; ee++)
	{
		size_t eelen = strlen(envp[ee]);
		env_ptrs[ee] = (char*)(argenv_addr + str_data_off);
		memcpy(argenv_kbuf + str_data_off, envp[ee], eelen);
		str_data_off += eelen + 1;
	}
	
	KASSERT(str_data_off == (ssize_t)argenv_size);
	
	//Now that we've got the data into kernel-space, flip over and copy it into the new user-space.
	uintptr_t old_uspc = m_uspc_current();
	m_uspc_activate(mem->uspc);
	memcpy((void*)argenv_addr, argenv_kbuf, argenv_size);
	m_uspc_activate(old_uspc);
	
	//Successo
	*loaded_out = argenv_addr;
	kpage_free(argenv_kbuf, argenv_size);
	return 0;
}
