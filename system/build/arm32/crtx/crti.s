//crti.s
//Function prologues for init/fini functions in C runtime - 32-bit ARM version
//Bryan E. Topp <betopp@betopp.com> 2021

.section .init.i
	
//Make reference to _init_done so it doesn't get GC'd
.extern _init_done
.word _init_done
	
//Linked at the beginning of the .init section
.global _init
_init:

.section .fini.i

//Make reference to _fini_done so it doesn't get GC'd
.extern _fini_done
.word _fini_done

//Linked at the beginning of the .fini section
.global _fini
_fini:


