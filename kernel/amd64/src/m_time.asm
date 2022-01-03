;m_time.asm
;Timing functions on amd64
;Bryan E. Topp <betopp@betopp.com> 2021

section .text
bits 64

global m_time_tsc ;int64_t m_time_tsc(void);
m_time_tsc:
	rdtsc
	shl RDX, 32 ;RDTSC puts high-order 32 bits into EDX
	or RAX, RDX
	ret
	
