;m_atomic.asm
;Atomic operations on AMD64
;Bryan E. Topp <betopp@betopp.com> 2021

section .text
bits 64

global m_atomic_increment_and_fetch ;m_atomic_t m_atomic_increment_and_fetch(volatile m_atomic_t *atomic);
m_atomic_increment_and_fetch:
	mov RAX, [RDI]
	mov RCX, RAX
	inc RCX
	lock cmpxchg [RDI], RCX
	jnz .tryagain
		mov RAX, RCX
		ret
	.tryagain:
	pause
	jmp m_atomic_increment_and_fetch	

global m_atomic_decrement_and_fetch ;m_atomic_t m_atomic_decrement_and_fetch(volatile m_atomic_t *atomic);
m_atomic_decrement_and_fetch:
	mov RAX, [RDI]
	mov RCX, RAX
	dec RCX
	lock cmpxchg [RDI], RCX
	jnz .tryagain
		mov RAX, RCX
		ret
	.tryagain:
	pause
	jmp m_atomic_decrement_and_fetch
