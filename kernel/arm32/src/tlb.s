//tlb.s
//TLB invalidation for 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

.section .text

.global _arm_invl //void _arm_invl(void)
_arm_invl:
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0 //Invalidate Instruction Cache
	mcr p15, 0, r0, c7, c5, 6 //Invalidate branch prediction array
	mcr p15, 0, r0, c8, c7, 0 //Invalidate entire Unified Main TLB
	bx lr

.global _arm_invlpg //void _arm_invlpg(uint32_t vaddr)
_arm_invlpg:
	mcr p15, 0, r0, c8, c7, 1
	bx lr

.global _arm_mbarrier //void _arm_mbarrier(void)
_arm_mbarrier:
	bx lr



.global m_uspc_current //m_uspc_t m_uspc_current()
m_uspc_current:
	//Compare with kernel tables - return 0 if they're in use
	.extern _ktrantable
	.extern _KSPACE_BASE
	ldr r1, =_ktrantable
	ldr r2, =_KSPACE_BASE
	sub r1, r1, r2
	
	mrc p15, 0, r0, c2, c0, 0 //Read TTBR
	cmp r0, r2
	beq m_uspc_current.ktable
		bx lr
	m_uspc_current.ktable:
		sub r0, r0
		bx lr

.global m_uspc_activate //void m_uspc_activate(m_uspc_t uspc)
m_uspc_activate:
	cmp r0, #0
	beq m_uspc_activate.ktable
		mcr p15, 0, r0, c2, c0, 0 //Write TTBR
		bx lr
	m_uspc_activate.ktable:
		.extern _ktrantable
		.extern _KSPACE_BASE
		ldr r0, =_ktrantable
		ldr r1, =_KSPACE_BASE
		sub r0, r0, r1
		mcr p15, 0, r0, c2, c0, 0 //Write TTBR
		bx lr