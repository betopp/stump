//crtn.s
//Function epilogues for init/fini functions in C runtime - 32-bit ARM version
//Bryan E. Topp <betopp@betopp.com> 2021

.section .init.n
	
//Linked at the end of the .init section
.global _init_done
_init_done:
bx lr

.section .fini.n

//Linked at the end of the .fini section
.global _fini_done
_fini_done:
bx lr

