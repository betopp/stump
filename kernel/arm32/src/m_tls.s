//m_tls.s
//TLS pointer storage on 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

.section .text

//We're single-processor for now, so we can just put this in memory.

.global m_tls_set //void m_tls_set(void *ptr);
m_tls_set:
	ldr r1, =_tls_storage
	str r0, [r1]
	bx lr

.global m_tls_get //void *m_tls_get(void);
m_tls_get:
	ldr r0, =_tls_storage
	ldr r0, [r0]
	bx lr

.section .data
	
.balign 4
_tls_storage:
	.space 4
