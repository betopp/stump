//popen.c
//linuxdoom uses this
//Bryan E. Topp <betopp@betopp.com> 2021

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <stdbool.h>

FILE *popen(const char *command, const char *type)
{
	FILE *retval = NULL;
	int pipe_tochild[2] = {-1, -1};
	int pipe_fromchild[2] = {-1, -1};
	
	bool type_r = false;
	bool type_w = false;
	bool type_plus = false;
	bool type_e = false;
	for(const char *tt = type; *tt != '\0'; tt++)
	{
		if(*tt == 'r')
			type_r = true;
		if(*tt == 'w')
			type_w = true;
		if(*tt == '+')
			type_plus = true;
		if(*tt == 'e')
			type_e = true;
	}
	
	if( (type_r && type_w) || (type_plus) )
	{
		//Don't have a way to combine the two pipes into a bidirectional pipe FD yet
		errno = ENOSYS;
		return NULL;
	}
	
	if(type_r || type_plus)
	{
		if(pipe2(pipe_fromchild, type_e ? O_CLOEXEC : 0) < 0)
			goto failure;
	}
	
	if(type_w || type_plus)
	{
		if(pipe2(pipe_tochild, type_e ? O_CLOEXEC : 0) < 0)
			goto failure;
	}

	char * const argv[] = {"/bin/sh", "-c", (char*)command, NULL};
	pid_t forkpid = fork();
	if(forkpid < 0)
	{
		//Error
		goto failure;
	}
	
	if(forkpid == 0)
	{
		//Child. Close existing stdin/stdout and redirect to pipes.
		close(STDIN_FILENO);
		dup2(pipe_tochild[0], STDIN_FILENO);
		
		close(STDOUT_FILENO);
		dup2(pipe_fromchild[1], STDOUT_FILENO);
		
		close(pipe_tochild[0]);
		close(pipe_tochild[1]);
		close(pipe_fromchild[0]);
		close(pipe_fromchild[1]);

		execv(argv[0], argv);
		perror("execv");
		exit(-1);
	}
	
	int pipefd = -1;
	if(pipe_fromchild[0] != -1)
		pipefd = pipe_fromchild[0];
	else if(pipe_tochild[1] != -1)
		pipefd = pipe_tochild[1];
	
	if(pipefd == -1)
	{
		errno = EINVAL;
		goto failure;
	}
	
	retval = fdopen(pipefd, type_r ? "r" : "w");
	if(retval == NULL)
	{
		goto failure;
	}
	
	return retval;
	
failure:
	
	if(pipe_tochild[0] >= 0)
		close(pipe_tochild[0]);
	if(pipe_tochild[1] >= 0)
		close(pipe_tochild[1]);
	if(pipe_fromchild[0] >= 0)
		close(pipe_fromchild[0]);
	if(pipe_fromchild[1] >= 0)
		close(pipe_fromchild[1]);
	
	if(retval != NULL)
		fclose(retval);
	
	return NULL;
}