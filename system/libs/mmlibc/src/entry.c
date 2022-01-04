//entry.c
//libc entry point
//Bryan E. Topp <betopp@betopp.com> 2021

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sc.h>

char **_default_environ = (char*[]){ "stump=" BUILDVERSION , NULL };

void _libc_entry(int argc, char **argv, char **envp)
{
	//Count argv if we need to
	if(argc == 0)
	{
		while(argv != NULL && argv[argc] != NULL)
			argc++;
	}
	
	//Set aside initial environment location
	extern char **environ;
	if(envp != NULL)
		environ = envp;
	else
		environ = _default_environ;
	
	//Set signal actions to default
	for(int ss = 0; ss < 64; ss++)
	{
		signal(ss, SIG_DFL);
	}
	_sc_sig_mask(SIG_SETMASK, 0);
	
	//Call constructors
	extern void _init();
	_init();
	
	//Call main
	extern int main();
	int main_returned = main(argc, argv, envp);
	
	//If main returns, call exit with its return value.
	exit(main_returned);
	
	//Exit shouldn't return
	_Exit(main_returned);
	abort();
	while(1){}
}
