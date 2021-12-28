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
	;On AMD64, "sti" enables interrupts after execution of the following instruction.
	;So a sequence of sti/hlt will atomically "halt with interrupts enabled".
	sti
	hlt
	ret

global m_intr_wake ;void m_intr_wake(void);
m_intr_wake:

	;Get APIC base address in RAX (todo - can we assume this is constant? man I want x2apic with its own MSRs)
	mov ECX, 0x1B ;APIC BAR
	rdmsr
	and RAX, 0xFFFFF000
	and RDX, 0x000FFFFF
	shl RDX, 32
	or RAX, RDX
	
	;Trigger interprocessor interrupt
	lea RDI, [RAX + 0x300] ;interrupt command register low
	mov RSI, 0xC40FE ;Fixed interrupt, positive edge-trigger, to all-except-self, vector 0xFE
	extern pspace_write32
	call pspace_write32
	
	ret