//m_atomic.s
//Atomic operations on 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

.section .text




.global m_atomic_increment_and_fetch //m_atomic_t m_atomic_increment_and_fetch(volatile m_atomic_t *atomic);
m_atomic_increment_and_fetch:

	//For now we're on a uniprocessor system...
	ldr r1, [r0]
	add r1, #1
	str r1, [r0]
	mov r0, r1
	bx lr

	//ldrex r1, [r0] //Load old value
	//add r1, #1 //Increment old value
	//strex r2, r1, [r0] //Store incremented value - indicate success/failure in r2
	//cmp r2, #0 //Zero indicates success
	//bne m_atomic_increment_and_fetch //Try again on atomicity failure
	//mov r0, r1 //Return incremented value
	//bx lr

.global m_atomic_decrement_and_fetch //m_atomic_t m_atomic_decrement_and_fetch(volatile m_atomic_t *atomic);
m_atomic_decrement_and_fetch:

	//For now we're on a uniprocessor system...
	ldr r1, [r0]
	sub r1, #1
	str r1, [r0]
	mov r0, r1
	bx lr

	//ldrex r1, [r0] //Load old value
	//sub r1, #1 //Decrement old value
	//strex r2, r1, [r0] //Store decremented value - indicate success/failure in r2
	//cmp r2, #0 //Zero indicates success
	//bne m_atomic_decrement_and_fetch //Try again on atomicity failure
	//mov r0, r1 //Return decremented value
	//bx lr

.global m_atomic_cmpxchg //bool m_atomic_cmpxchg(volatile m_atomic_t *atomic, m_atomic_t oldv, m_atomic_t newv);
m_atomic_cmpxchg:

	//For now we're on a uniprocessor system...
	ldr r3, [r0]
	cmp r3, r1
	bne m_atomic_cmpxchg.fail
	
	str r2, [r0]


	//ldrex r3, [r0] //Load old value
	//cmp r3, r1 //Compare with expected value
	//bne m_atomic_cmpxchg.fail //Fail if it wasn't what we expected
	//strex r3, r2, [r0] //Store new value - indicate success/failure in r3
	//cmp r3, #0 //Zero indicates success
	//bne m_atomic_cmpxchg //Try again on atomicity failure
	
		mov r0, #1 //Success
		bx lr
	m_atomic_cmpxchg.fail:
		sub r0, r0 //Failure
		bx lr
		
