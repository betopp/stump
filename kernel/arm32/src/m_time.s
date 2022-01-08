//m_time.s
//Timing functions on 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

.section .text

.global m_time_tsc //int64_t m_time_tsc(void);
m_time_tsc:
	//ARM9 just... doesn't have this?
	ldr r2, =_faketsc
	ldr r0, [r2]
	ldr r1, [r2, #4]
	adds r0, #32768
	adc r1, #0
	str r0, [r2]
	str r1, [r2, #4]
	bx lr
	.ltorg

.section .data
	
.balign 8
_faketsc:
	.space 8
	