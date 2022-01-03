//sinit.c
//STUMP init program
//Bryan E. Topp <betopp@betopp.com> 2021

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sc.h>

extern char **environ;

int main(int argc, const char **argv, const char **envp)
{
	//Todo - could use this to pass the kernel commandline or something
	(void)argc;
	(void)argv;
	(void)envp;
	
	//Open the kernel logging device as our stdout/stderr.
	int log_fd = open("/dev/log", O_WRONLY);
	if(log_fd < 0)
	{
		perror("open /dev/log"); //Well, can't actually print the error...
		abort();
	}
	dup2(log_fd, STDOUT_FILENO);
	dup2(log_fd, STDERR_FILENO);
	
	//Open /dev/null as our stdin.
	int null_fd = open("/dev/null", O_RDONLY);
	if(null_fd < 0)
	{
		perror("open /dev/null");
		abort();
	}
	dup2(null_fd, STDIN_FILENO);
	
	//Give us a valid PWD
	int chdir_err = chdir("/");
	if(chdir_err < 0)
	{
		perror("chdir /");
		abort();
	}
	
	//Open the terminal emulator executable, and hold on to it
	int term_fd = open("/bin/sterm", O_RDONLY | O_EXEC);
	if(term_fd < 0)
	{
		perror("open /bin/sterm");
		abort();
	}
	
	//Set some environment variables
	setenv("HOME", "/", 0);
	
	//Keep terminal emulator spawned and active on the console
	pid_t term_pid = 0;
	while(1)
	{
		//If we don't have a terminal emulator, spawn one
		if(term_pid == 0)
		{
			pid_t forkpid = fork();
			if(forkpid < 0)
			{
				//Failed to fork
				perror("fork");
				abort();
			}
			
			if(forkpid == 0)
			{
				//Child - execute terminal emulator
				int exec_err = fexecve(term_fd, (char*[]){"sterm", NULL}, environ);
				(void)exec_err; //Must be error if fexecve returns.
				_Exit(-1);
			}
			
			term_pid = forkpid;
		}
		
		//If we've got the console, hand it to the terminal emulator.
		//(Note that we might get it back later, and have to pass it again.)
		_sc_con_pass(term_pid);
	
		//Check for child status updates. If one of those is the terminal emulator exiting, note that it's gone.
		int wait_status = 0;
		pid_t wait_pid = waitpid(0, &wait_status, WNOHANG);
		if(wait_pid < 0)
		{
			perror("wait");
			abort();
		}
		
		if(WIFEXITED(wait_status) || WIFSIGNALED(wait_status))
		{
			if(wait_pid == term_pid)
				term_pid = 0;
		}
		
		//Don't just spin if nothing is happening.
		_sc_pause();
	}
}
