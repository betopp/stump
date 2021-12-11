;m_intr.asm
;Interrupt control on AMD64
;Bryan E. Topp <betopp@betopp.com> 2021

global m_intr_ei ;void m_intr_ei(bool enable);
m_intr_ei:
	;Do you want the moustache on or off?
	cmp RDI, 0
	jne .enable
		cli
		ret
	.enable:
		sti
		ret

global m_intr_halt ;void m_intr_halt(void);
m_intr_halt:
	sti
	hlt
	ret
