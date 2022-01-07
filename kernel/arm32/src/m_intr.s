//m_intr.s
//Interrupt control on 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

.section .text

.global m_intr_ei //void m_intr_ei(bool enable);
m_intr_ei:

	//Check parameter
	cmp r0, #0
	bne m_intr_ei.enable
		//Set FIQ/IRQ mask bits to disable interrupts
		mrs r1, cpsr
		orr r1, r1, #0xc0
		msr cpsr_c, r1
		bx lr
		
	m_intr_ei.enable:
		//Clear FIQ/IRQ mask bits to enable interrupts
		mrs r1, cpsr
		bic r1, r1, #0xc0
		msr cpsr_c, r1
		bx lr


.global m_intr_halt //void m_intr_halt(void);
m_intr_halt:

	//We assume interrupts are disabled at this point.
	//Wait for any interrupt to become pending.
	//Note that if an interrupt is already pending, WFI does nothing.
	sub r0, r0, r0
	mcr p15,0,r0,c7,c0,4 //wfi 
	
	//Whether WFI actually waited or not, enable interrupts to catch the pending interrupt.
	mrs r1, cpsr
	bic r1, r1, #0xc0
	msr cpsr_c, r1
	bx lr

.global m_intr_wake //void m_intr_wake(void);
m_intr_wake:

	//Stub because we're single-processor for now
	bx lr
