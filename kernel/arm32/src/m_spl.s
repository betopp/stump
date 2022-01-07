//m_spl.s
//Spinlocks on 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

.section .text
	
//Dummy for now, as we're on a single-core ARM with an architecture too old for ldrex/strex	

.global m_spl_acq //void m_spl_acq(m_spl_t *spl);
m_spl_acq:
	bx lr

.global m_spl_try //bool m_spl_try(m_spl_t *spl);
m_spl_try:
	mov r0, #1
	bx lr

.global m_spl_rel //void m_spl_rel(m_spl_t *spl);
m_spl_rel:
	bx lr

	
	