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

global m_intr_wake ;void m_intr_wake(void);
m_intr_wake:

	;Get APIC base address in RAX (todo - can we assume this is constant? man I want x2apic with its own MSRs)
	mov ECX, 0x1B ;APIC BAR
	rdmsr
	shl RDX, 32
	or RAX, RDX
	and RAX, 0x00FFFFFFFFFFF000
	
	;Trigger interprocessor interrupt
	lea RDI, [RAX + 0x300] ;interrupt command register low
	mov RSI, 0xC45FF ;Fixed interrupt, positive edge-trigger, to all-except-self, vector 0xFF
	extern pspace_write
	call pspace_write
	
	ret