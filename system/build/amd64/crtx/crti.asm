;crti.asm
;Function prologues for init/fini functions in C runtime
;Bryan E. Topp <betopp@betopp.com> 2021

bits 64
section .init.i
	
;Make reference to _init_done so it doesn't get GC'd
extern _init_done
dq _init_done
	
;Linked at the beginning of the .init section
global _init
_init:
push RBP
mov RBP, RSP


bits 64
section .fini.i

;Make reference to _fini_done so it doesn't get GC'd
extern _fini_done
dq _fini_done

;Linked at the beginning of the .fini section
global _fini
_fini:
push RBP
mov RBP, RSP

