//m_time.s
//Timing functions on 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

.section .text

.global m_time_tsc //int64_t m_time_tsc(void);
m_time_tsc:
	