//arm32ini.s
//User context for 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

.section .text

.global m_drop_copy //void m_drop_copy(m_drop_t *dst, const m_drop_t *src);
m_drop_copy:
	sub r3, r3
	.loop:
		ldr r2, [r1, r3]
		str r2, [r0, r3]
		add r3, #4
		cmp r3, #(17*4)
		blt .loop
	bx lr

.global m_drop_reset //void m_drop_reset(m_drop_t *drop, uintptr_t entry);
m_drop_reset:

	//Reset GPR state
	sub r2, r2
	str r2, [r0, #(4* 0)]
	str r2, [r0, #(4* 1)]
	str r2, [r0, #(4* 2)]
	str r2, [r0, #(4* 3)]
	str r2, [r0, #(4* 4)]
	str r2, [r0, #(4* 5)]
	str r2, [r0, #(4* 6)]
	str r2, [r0, #(4* 7)]
	str r2, [r0, #(4* 8)]
	str r2, [r0, #(4* 9)]
	str r2, [r0, #(4*10)]
	str r2, [r0, #(4*11)]
	str r2, [r0, #(4*12)]
	
	//Reset SP and LR
	str r2, [r0, #(4*13)]
	str r2, [r0, #(4*14)]
	
	//Store initial PC
	str r1, [r0, #(4*15)]
	
	//Reset PSR and make it indicate user-mode with interrupts enabled
	mov r2, #0x10
	str r2, [r0, #(4*16)]
	
	bx lr

.global m_drop_retval //void m_drop_retval(m_drop_t *drop, uintptr_t retval);
m_drop_retval:
	str r1, [r0, #(4* 0)]
	bx lr

.global m_drop_signal //void m_drop_signal(m_drop_t *drop, uintptr_t pc, uintptr_t sp);
m_drop_signal:
	str r1, [r0, #(4*15)]
	str r2, [r0, #(4*13)]
	bx lr

.global m_drop //void m_drop(const m_drop_t *drop) __attribute__((noreturn));
m_drop:
	//Transfer to supervisor mode before dropping back to user mode
	//(This is needed so ldmia loads into the right bank of registers)
	mov r1, #0xDF //System-mode with interrupts disabled
	msr cpsr_c, r1 //Switch mode

	//Put the saved PSR from the drop-buffer into our current mode SPSR
	ldr r1, [r0, #(4*16)]
	msr spsr, r1
	
	//Restore all registers and load CPSR from SPSR
	ldmia r0, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, sp, lr, pc}^
	.ltorg
