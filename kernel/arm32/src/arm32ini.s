//arm32ini.s
//Entry point for 32-bit ARM kernel
//Bryan E. Topp <betopp@betopp.com> 2021

.section .boot //Mapped at 0 by linker

//Entry point for hardware setup
.global _start
_start:

	//Use initial stack
	ldr sp, =_stack.top
	
	//Subtrahend to make physical addresses from kernel virtual addresses
	ldr r5, =_KSPACE_BASE
	
	//Zero the BSS region as-linked
	ldr r1, =_BSS_START
	sub r1, r1, r5
	ldr r2, =_BSS_END
	sub r2, r2, r5
	sub r3, r3, r3
	_start.bssloop:
		str r3, [r1]
		add r1, #4
		cmp r1, r2
		blt _start.bssloop
	
	//Load translation table base (c2)
	ldr r1, =_ktrantable //Base address of translation table
	sub r1, r1, r5
	mcr p15, 0, r1, c2, c0, 0 //Write TTBR
	
	//Set all MMU domains (c3) to "client" so permissions are always checked.
	ldr r1, =#0x55555555 //0b01 in each of 16 fields
	mcr p15, 0, r1, c3, c0, 0 //Write domain access permissions
	
	//Set S bit and clear R bit in control register (c1).
	//This causes pages with 00 access permission to be read-only in kernel and denied to user.
	//Clear bit 23, to get ARMv5 pagetable format (w/subpages).
	mrc p15, 0, r1, c1, c0, 0 //Read control register
	orr r1, r1, #(1<<8) //set S
	and r1, r1, #(~(1<<9)) //clear R
	and r1, r1, #(~(1<<23)) //clear b23
	mcr p15, 0, r1, c1, c0, 0 //Write control register
	
	//Set up kernel page tables at the top and bottom of the translation table.
	//Top 256 entries point to coarse page tables, giving 256MBytes of kernel space.
	//Identity-map it, and map it at the top too.
	ldr r1, =_ktrantable //Base address of translation table
	sub r1, r1, r5
	add r4, r1, #(4*4096) //Top of table
	sub r4, r4, #(4*256) //Minus 256 megs
	ldr r2, =_KSPACE_PHYS //Base of identity-mapping
	mov r2, r2, lsr #(20-2) //...as offset in table
	add r1, r2
	ldr r2, =_kpagetables //Base address of each kernel page table
	sub r2, r2, r5
	orr r2, r2, #0x11 //Domain 0, points to coarse page table
	mov r3, #256 //Fill in 256 entries
	_start.ttloop:
		str r2, [r1] //Fill in the entry, identity-mapped
		str r2, [r4] //Fill in the entry, virtual
		add r1, r1, #4 //Advance location being filled-in, identity-mapped
		add r4, r4, #4 //Advance location being filled-in, virtual
		add r2, r2, #1024 //Advance location pointed-to
		sub r3, r3, #1 //Count down loops remaining
		cmp r3, #0
		bne _start.ttloop
		
	//Set up kernel page tables with mapping to cover kernel as linked
	ldr r1, =_kpagetables //Base address of page tables
	sub r1, r1, r5
	ldr r2, =_KERNEL_START //Starting address to map
	sub r2, r2, r5
	ldr r3, =#0xFFFFFFF //256 megs of tables in total
	and r2, r2, r3
	mov r3, r2, lsr #12 //Starting page
	add r1, r3, lsl #2 //Offset starting location in page tables
	
	ldr r2, =_KERNEL_START //Starting address to map
	ldr r3, =_KERNEL_END //Address to stop mapping
	sub r3, r3, r2 //Turn into "how many addresses"
	mov r3, r3, lsr #12 //Turn into "how many pages"
	
	ldr r2, =_KERNEL_START //Starting address to map
	sub r2, r2, r5
	orr r2, r2, #0xE //Writeback cacheable small page
	orr r2, r2, #0x550 //All subpages kernel-only
	
	_start.ptloop:
		str r2, [r1] //Fill in the entry
		add r1, r1, #4 //Advance location being filled-in
		add r2, r2, #4096 //Advance location pointed-to
		sub r3, r3, #1 //Count down loops remaining
		cmp r3, #0
		bne _start.ptloop
		
	//Enable MMU
	mrc p15, 0, r1, c1, c0, 0 //Read control register
	orr r1, r1, #(1<<0)  //set M
	orr r1, r1, #(1<<12) //set I
	orr r1, r1, #(1<<13) //set V (map vector table to 0xFFFF0000)
	mcr p15, 0, r1, c1, c0, 0 //Write control register

	//Symbols are now usable in proper, virtual space.

	//Jump to virtual space and unmap identity-mapped code
	ldr r1, =_start.vtarget
	bx r1
	_start.vtarget:
	
	ldr r1, =_ktrantable //Base address of translation table
	ldr r3, = #(4096-256) //All but last 256 entries (256MBytes virtual kernel space)
	sub r0, r0, r0
	_start.clearloop:
		str r0, [r1] //Clear the entry
		add r1, r1, #4 //Advance location being filled-in
		sub r3, r3, #1 //Count down loops remaining
		cmp r3, #0
		bne _start.clearloop
		
	//Map vector table where it needs to be at top of virtual space
	ldr r1, =_kpagetables.end //Top of kernel page tables
	sub r1, r1, #(16*4) //Back up 16 pages, to 0xFFFF0000
	ldr r2, =_vectors //We'll add an additional mapping to this space, here
	sub r2, r5
	orr r2, r2, #0xE //Writeback cacheable small page
	orr r2, r2, #0x550 //All subpages kernel-only
	str r2, [r1]
	
	//Initialize machine-specific bits
	.extern _hw_init
	ldr r1, =_hw_init
	blx r1

	//Run C kernel code, in virtual space
	.extern entry_boot
	ldr r1, =entry_boot
	blx r1
	
	.extern entry_smp
	ldr r1, =entry_smp
	blx r1
	
	b _start
	
.ltorg

//Standard handling of exceptions
.macro EXCENTRY parm:req
	//Set up kernel stack pointer - top of kernel stack
	ldr sp, =_stack.top
	
	//Put most of the interrupted state on the kernel stack.
	//Note that this saves the kernel SP and LR which we don't actually want.
	sub sp, #(24*4)
	stmia sp, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, sp, lr, pc}
	
	//Save the address of the aborted instruction as the PC on the stack (processor puts it in our banked LR)
	//Note - needs adjustment by -4 or -8 depending on type of fault
	str lr, [sp, #(4*15)]
	
	//Save the PSR, from the interrupted context (set aside in SPSR already)
	mrs r0, spsr
	str r0, [sp, #(4*16)]
	
	//Make sure the interrupted state was user-mode.
	and r0, #0x1F
	cmp r0, #0x10
	blne _exc_not_usermode
	
	//Get the SP/LR registers from user-mode. These are also accessible in system mode.
	//Switch to system-mode so we can grab them and switch back.
	mrs r1, cpsr //Preserve mode that we're in now
	mov r0, #0xDF //System-mode with interrupts disabled
	msr cpsr_c, r0 //Switch mode
	mov r3, sp //Get user/system-mode SP
	mov r4, lr //Get user/system-mode LR
	msr cpsr_c, r1 //Switch back from system-mode
	
	//Store the SP and LR registers retrieved from user-mode state, now that we're back on kernel SP
	str r3, [sp, #(4*13)]
	str r4, [sp, #(4*14)]

	//We've now preserved the old state. Pass a pointer and handle the rest in C.
	.extern _call_exc //void _call_exc(int which, m_drop_t *interrupted, uintptr_t dfault)
	mov r0, \parm
	mov r1, sp
	mrc p15, 0, r2, c6, c0, 0 //read Fault Address Register
	ldr r5, =_call_exc
	bx r5
	
	.ltorg
.endm


//Exception handlers - build a drop-buffer and enter kernel.
//We don't take exceptions in kernel mode.
_exc_reset:
	EXCENTRY #0

_exc_ud:
	EXCENTRY #1

_exc_svc:
	EXCENTRY #2

_exc_pf:
	EXCENTRY #3
	
_exc_dat:
	EXCENTRY #4
	
_exc_unused:
	EXCENTRY #5
	
_exc_irq:
	ldr sp, =_irqstack.top //Banked register - not clobbering user's
	sub lr, #4 //Old PC in banked LR
	stmfd sp!, {r0-r3, r9, r12, lr}
	.extern _hw_irq
	bl _hw_irq
	ldmfd sp!, {r0-r3, r9, r12, pc}^ //Returns to interrupted mode, restoring SPSR->CPSR
	
_exc_fiq:
	ldr sp, =_fiqstack.top //Banked register - not clobbering user's
	sub lr, #4 //Old PC in banked LR
	stmfd sp!, {r0-r3, r9, r12, lr}
	.extern _hw_fiq
	bl _hw_irq
	ldmfd sp!, {r0-r3, r9, r12, pc}^ //Returns to interrupted mode, restoring SPSR->CPSR

//Panic location if an exception is taken from non-user-mode
_exc_not_usermode:
	b _exc_not_usermode
	
.ltorg

//Vector table - page gets duplicate mapping in the right place at runtime
.balign 16384
.global _vectors
_vectors:
	ldr pc, =_exc_reset  //0 - Reset
	ldr pc, =_exc_ud     //1 - Undefined instruction
	ldr pc, =_exc_svc    //2 - Syscall
	ldr pc, =_exc_pf     //3 - Prefetch abort
	ldr pc, =_exc_dat    //4 - Data abort
	ldr pc, =_exc_unused //5 - Unused
	ldr pc, =_exc_irq    //6 - IRQ
	ldr pc, =_exc_fiq    //7 - FIQ
	.ltorg
.balign 16384

.section .bss

//Space for kernel top-level translation table (4096 entries of 4B each)
.balign 16384
.global _ktrantable
_ktrantable:
	.space 16384

//Space for kernel page tables - 256 coarse page tables of 256x4B entries each.
//Covers 16MByte of kernel space. Mapped contiguously at the top of memory.
.balign 16384
.global _kpagetables
_kpagetables:
	.space 262144
	_kpagetables.end:
	
//Space for mapping the head of the free-list of frames.
//When frames are allocated/freed, this is remapped to always point at the head.
.balign 16384
.global _freelist
_freelist:
	.space 16384
	
//Initial stack space
.balign 16384
_stack:
	.space 16384
	_stack.top:
	
//Stack for IRQ
.balign 16384
_irqstack:
	.space 16384
	_irqstack.top:
	
//Stack for FIQ
.balign 16384
_fiqstack:
	.space 16384
	_fiqstack.top:

