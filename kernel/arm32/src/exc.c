//exc.c
//Exception handling on ARM32 - in C so we can use headers
//Bryan E. Topp <betopp@betopp.com> 2021

//#include "m_exc.h"
#include "m_drop.h"
#include "m_panic.h"

//Kernel symbol called when entering from user-mode
//extern void entry_exc(m_drop_t *drop, m_exc_type_t exc, uintptr_t badaddr) __attribute__((noreturn));

//Kernel symbol called when making a system call
extern uintptr_t entry_syscall(uintptr_t num, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, uintptr_t p5, m_drop_t *drop);

//Uniform handler for exceptions to make the assembly less godawful.
//Called after the user-mode state has been preserved in the given drop-buffer.
void _call_exc(int which, m_drop_t *interrupted, uintptr_t dfault)
{
	(void)dfault;
	
	switch(which)
	{
		case 0: //Reset
			m_panic("reset");
		
		case 1: //Undefined instruction
			m_panic("undefined op");
		
		case 2: //Syscall
			entry_syscall(
				interrupted->words[0],
				interrupted->words[1],
				interrupted->words[2],
				interrupted->words[3],
				interrupted->words[4],
				interrupted->words[5],
				interrupted
			); //Shouldn't return
			m_panic("syscall");
		
		case 3: //Prefetch abort
			//Stored PC is actually faulting instruction + 4
			interrupted->words[15] -= 4;
			//entry_exc(interrupted, M_EXC_PAGEFAULT, interrupted->words[15]); //shouldn't return
			m_panic("prefetch abort");
		
		case 4: //Data abort
			//Stored PC is actually faulting instruction + 8
			interrupted->words[15] -= 8;
			//entry_exc(interrupted, M_EXC_PAGEFAULT, dfault); //shouldn't return
			m_panic("data abort");
		
		case 5: //Unused
			m_panic("unused exc");
		
		case 6: //IRQ - not handled here, see arm32ini.s
			m_panic("irq");
		
		case 7: //FIQ - not handled here, see arm32ini.s
			m_panic("fiq");
		
		default:
			m_panic("bad exc");
	}
}
