//crt0.s
//Entry point for C runtime - 32-bit ARM version
//Bryan E. Topp <betopp@betopp.com> 2021

.section .text
	
//Entry point
.align 16
.global _crt_entry
_crt_entry:

	//Set up TLS and stack for initial thread
	ldr r9, =_crt_tls0
	ldr sp, =_crt_stack.top
	
	//r0 points to our argv/envp pointers. Preserve it.
	mov r4, r0
	
	//Set signal entry/stack for initial thread
	.extern _sc_sig_entry
	ldr r0, =_crt_sigentry
	ldr r1, =_crt_sigstack.top
	blx _sc_sig_entry
	
	//Restore arg/env pointers and load them up as parameters to entry.
	mov r0, r4
	cmp r0, #0
	beq _crt_entry.after_args
	ldr r1, [r0]
	ldr r2, [r0, #4]
	mov r0, #0 //Let libc entry count them
	_crt_entry.after_args:
	
	//Call libc which will call main and then exit
	.extern _libc_entry
	blx _libc_entry

	//Should never return
	_crt_entry.spin:
	b _crt_entry.spin
	
	.ltorg
	
//Entry for signal handler
_crt_sigentry:
	
	//Handle signal
	.extern _libc_signalled
	blx _libc_signalled

	//Return to context saved by the kernel
	.extern _sc_sig_return
	blx _sc_sig_return

	
//System call shims
.global _sc0
.global _sc1
.global _sc2
.global _sc3
.global _sc4
.global _sc5
_sc0:
_sc1:
_sc2:
_sc3:
_sc4:
_sc5:
	//Parameters 0, 1, 2, and 3 are already passed in registers.
	//Parameters 4 and 5, if used, are on the stack. Also stuff 4 and 5 into registers.
	//Preserve the old values of r4 and r5 in doing so.
	stmfd sp!, {r4, r5}
	ldr r4, [sp, #8]
	ldr r5, [sp, #12]
	svc 0
	ldmfd sp!, {r4, r5}
	bx lr

//longjumps
.global _setjmp //extern int _setjmp(jmp_buf env);
_setjmp:

	//These always happen as part of a function call.
	//So, only registers preserved across function calls must be saved/loaded.
	stmia r0!, {r4-r8, r10, r11, sp, lr} 
	
	//First return - _setjmp returns 0.
	sub r0, r0
	bx lr
	
.global _longjmp //extern void _longjmp(jmp_buf env, int val);
_longjmp:

	//Restore saved registers
	ldmia r0!, {r4-r8, r10, r11, sp, lr} 
	
	//Return as if _setjmp is now returning "val".
	//But - ensure that's not 0, because that means _setjmp is returning the first time.
	mov r0, r1
	cmp r0, #0
	bne .noinc
		add r0, #1
	.noinc:
	bx lr

.global sigsetjmp //int sigsetjmp(sigjmp_buf env, int savemask)
sigsetjmp:
	//store savemask value to env[0]
	str r1, [r0]
	
	//if savemask is nonzero, get signals to save.
	mov r2, #0 //default to saving nothing
	cmp r1, #0 //check savemask...
	beq sigsetjmp.nosigs	
		push {r0} //Preserve env ptr across call to _sc_sigmask
		
		.extern _sc_sig_mask //System call to alter/get signal mask
		mov r0, #0 //SIG_BLOCK - but we're passing a mask of zeroes, so nothing is blocked
		mov r1, #0 //Block nothing
		blx _sc_sig_mask
		
		mov r2, r0 //Saved signals returned in r0, we want them in r2
		pop {r0} //Restore env ptr
	sigsetjmp.nosigs:
	
	//Store the signal mask values in env[1]
	str r2, [r0, #4]
	
	//Continue to preserving registers in env[2...]
	add r0, #8
	b _setjmp


.global siglongjmp //void siglongjmp(sigjmp_buf env, int val)
siglongjmp:
	//get old value of savemask from env[0] - figure out if we'll restore signals
	ldr r2, [r0]
	cmp r2, #0
	beq siglongjmp.nosigs
		push {r0, r1} //Preserve across call to _sc_sigmask
		
		.extern _sc_sig_mask //System call to alter/get signal mask
		ldr r1, [r0, #4] //Mask preserved in env[1]
		mov r0, #2 //SIG_SETMASK
		blx _sc_sig_mask
		
		pop {r0, r1}
	siglongjmp.nosigs:
	
	//Do the rest of the jump from env[2...]
	add r0, #8
	b _longjmp
	

//TLS and spinlock implementation
.global _tls
_tls:
	mov r0, r9
	bx lr

//Single-threaded for now...
.global _spl_lock
_spl_lock:
	mov r1, #1
	str r1, [r0]
	bx lr

.global _spl_unlock
_spl_unlock:
	mov r1, #0
	str r1, [r0]
	bx lr

	

.section .bss
	
//Space for stack
.balign 4096
_crt_stack:
	.space 4096 * 8
	_crt_stack.top:	

//Space for signal-handling stack
.balign 4096
_crt_sigstack:
	.space 4096 * 8
	_crt_sigstack.top:
	
.section .data
	
//Space for initial thread TLS
.balign 4096
_crt_tls0:
	.word _crt_tls0 //TLS starts with pointer to itself
	.space (4096 - 4)
	_crt_tls0.top:
	
