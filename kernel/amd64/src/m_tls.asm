;m_tls.asm
;TLS pointer storage on AMD64
;Bryan E. Topp <betopp@betopp.com> 2021

section .text
bits 64

;We use the GS-base register to hold the TLS pointer.
;We never actually access through [GS:...] though. It's just a handy extra register.

global m_tls_set ;void m_tls_set(void *ptr);
m_tls_set:
	wrgsbase RDI
	ret

global m_tls_get ;void *m_tls_get(void);
m_tls_get:
	rdgsbase RAX
	ret

