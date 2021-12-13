//sinit.c
//STUMP init program
//Bryan E. Topp <betopp@betopp.com> 2021

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

//Number of terminals we spawn
#define TERM_MAX 1
pid_t term_pids[TERM_MAX];

int main(int argc, const char **argv, const char **envp)
{
	//Todo - could use this to pass the kernel commandline or something
	(void)argc;
	(void)argv;
	
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
	
	//Keep terminal emulators spawned
	while(1)
	{
		//Spawn any that don't exist.
		for(int tt = 0; tt < TERM_MAX; tt++)
		{
			if(term_pids[tt] != 0)
				continue; //Already spawned this one
			
			//Name of console device that the terminal emulator will attach to
			char con_name[16] = {0};
			snprintf(con_name, sizeof(con_name) - 1, "/dev/con%d", tt);
			
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
				int exec_err = fexecve(term_fd, (char*[]){"sterm", con_name, NULL}, (char**)envp);
				(void)exec_err; //Must be error if fexecve returns.
				_Exit(-1);
			}
			
			//Note the process ID of the child for this terminal emulator.
			term_pids[tt] = forkpid;
		}
		
		//Wait for any child processes that have status.
		//If one of those is a terminal emulator exiting, note that.
		int wait_status = 0;
		pid_t wait_pid = wait(&wait_status);
		if(wait_pid < 0)
		{
			perror("wait");
			abort();
		}
		
		if(WIFEXITED(wait_status) || WIFSIGNALED(wait_status))
		{
			for(int tt = 0; tt < TERM_MAX; tt++)
			{
				if(wait_pid == term_pids[tt])
					term_pids[tt] = 0;
			}
		}
	}
}