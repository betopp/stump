;m_spl.asm
;Spinlock functions on AMD64
;Bryan E. Topp <betopp@betopp.com> 2021

section .text
bits 64

global m_spl_acq ;void m_spl_acq(m_spl_t *spl);
m_spl_acq:
	mov AX, 0x0100 ;AL = 0, AH = 1.
	lock cmpxchg [RDI], AH ;Compares AL (0) with the spinlock. If it's 0, AH (1) is stored.
	jnz .wait ;If we failed, ZF is cleared - wait and try again
		mfence ;Make sure all prior memory operations complete before we say we "have the lock".
		ret
	.wait:
		;Didn't get the lock.
		;Use non-locked accesses to wait until the lock seems to be 0. Then try again.
		pause
		cmp byte [RDI], 0
		jne m_spl_acq
		jmp m_spl_acq

global m_spl_try ;bool m_spl_try(m_spl_t *spl);
m_spl_try:
	mov AX, 0x0100 ;AL = 0, AH = 1.
	lock cmpxchg [RDI], AH ;Compares AL (0) with the spinlock. If it's 0, AH (1) is stored.
	jnz .failed ;If we failed, ZF is cleared - return false
		mfence ;Make sure all prior memory operations complete before we say we "have the lock".
		mov RAX, 1 ;Return true
		ret
	.failed:
		mov RAX, 0 ;Return false
		ret

global m_spl_rel ;void m_spl_rel(m_spl_t *spl);
m_spl_rel:
	;Just set the lock value to 0
	mov byte [RDI], 0
	ret
