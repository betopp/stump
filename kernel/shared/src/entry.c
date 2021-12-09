//entry.c
//Entry points to kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "kpage.h"
#include "ramfs.h"
#include "fb.h"

//Entered once on bootstrap core. Should set up kernel and return.
void entry_boot(void)
{
	//Set up in-memory systems
	kpage_init();
	ramfs_init();
	fb_init();
}

//Entered on all cores once entry_one returns. Should schedule threads and never return.
void entry_smp(void)
{
	while(1) { }
}
