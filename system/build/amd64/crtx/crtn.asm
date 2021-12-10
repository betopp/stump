;crtn.asm
;Function epilogues for init/fini functions in C runtime
;Bryan E. Topp <betopp@betopp.com> 2021

bits 64
section .init.n
	
;Linked at the end of the .init section
global _init_done
_init_done:
pop RBP
ret


bits 64
section .fini.n

;Linked at the end of the .fini section
global _fini_done
_fini_done:
pop RBP
ret

