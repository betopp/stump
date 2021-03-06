;crt0.asm
;Entry point for C runtime
;Bryan E. Topp <betopp@betopp.com> 2021

bits 64
section .text
	
;Entry point
align 16
global _crt_entry
_crt_entry:

	;Set up TLS and stack for initial thread
	mov RCX, _crt_tls0
	wrgsbase RCX
	
	mov RSP, _crt_stack.top
	
	;RAX points to our argv/envp pointers. Preserve it.
	mov RBX, RAX
	
	;Set signal entry/stack for initial thread
	extern _sc_sig_entry
	mov RDI, _crt_sigentry
	mov RSI, _crt_sigstack.top
	call _sc_sig_entry
	
	;Restore arg/env pointers and load them up as parameters to entry.
	mov RAX, RBX
	cmp RAX, 0
	je .after_args
	mov RDI, 0 ;Let libc entry count them
	mov RSI, [RAX]
	mov RDX, [RAX + 8]
	.after_args:
	
	;Call libc which will call main and then exit
	extern _libc_entry
	call _libc_entry

	;Should never return
	hlt
	.spin:
	jmp .spin
	
	
;Entry for signal handler
_crt_sigentry:
	
	;Handle signal
	extern _libc_signalled
	call _libc_signalled

	;Return to context saved by the kernel
	extern _sc_sig_return
	call _sc_sig_return

	
;System call handlers.
;Each requires one more parameter than the parameters to the call (i.e. which call are we running).
;If we need to pass a fourth parameter, it can't go in RCX as normal - RCX gets clobbered by syscall.
global _sc0
_sc0:
	syscall
	ret
	
global _sc1
_sc1:
	syscall	
	ret
	
global _sc2
_sc2:
	syscall
	ret
	
global _sc3
_sc3:
	mov R10, RCX
	syscall
	ret
	
global _sc4
_sc4:
	mov R10, RCX
	syscall
	ret
	
global _sc5
_sc5:	
	mov R10, RCX
	syscall
	ret

;longjumps
global _setjmp ;extern int _setjmp(jmp_buf env);
_setjmp:

	;These always happen as part of a function call.
	;So, only registers preserved across function calls must be saved/loaded.
	mov [RDI + (8 * 0)], RBX
	mov [RDI + (8 * 1)], RSP
	mov [RDI + (8 * 2)], RBP
	mov [RDI + (8 * 3)], R12
	mov [RDI + (8 * 4)], R13
	mov [RDI + (8 * 5)], R14
	mov [RDI + (8 * 6)], R15
	
	;Preserve return address from stack - when _setjmp returns, it can be clobbered by further calls.
	mov RAX, [RSP]
	mov [RDI + (8 * 7)], RAX
	
	;First return - _setjmp returns 0.
	mov RAX, 0
	ret
	
global _longjmp ;extern void _longjmp(jmp_buf env, int val);
_longjmp:

	;Restore saved registers
	mov RBX, [RDI + (8 * 0)]
	mov RSP, [RDI + (8 * 1)]
	mov RBP, [RDI + (8 * 2)]
	mov R12, [RDI + (8 * 3)]
	mov R13, [RDI + (8 * 4)]
	mov R14, [RDI + (8 * 5)]
	mov R15, [RDI + (8 * 6)]
	
	;Restore return-address that was in use when _setjmp was called
	mov RAX, [RDI + (8 * 7)]
	mov [RSP], RAX
	
	;Return as if _setjmp is now returning "val".
	;But - ensure that's not 0, because that means _setjmp is returning the first time.
	mov RAX, RSI
	cmp RAX, 0
	jne .noinc
		inc RAX
	.noinc:
	ret

global sigsetjmp ;int sigsetjmp(sigjmp_buf env, int savemask)
sigsetjmp:
	;store savemask value to env[0]
	mov [RDI], RSI
	
	;if savemask is nonzero, get signals to save.
	cmp RSI, 0
	mov RAX, 0
	je .nosigs	
		push RDI ;Preserve across call to _sc_sigmask
		
		extern _sc_sig_mask ;System call to alter/get signal mask
		mov RDI, 0 ;SIG_BLOCK - but we're passing a mask of zeroes, so nothing is blocked
		mov RSI, 0 ;Block nothing
		call _sc_sig_mask
		
		pop RDI
	.nosigs:
	
	;Store the signal mask values in env[1]
	mov [RDI + 8], RAX
	
	;Continue to preserving registers in env[2...]
	add RDI, 16
	jmp _setjmp


global siglongjmp ;void siglongjmp(sigjmp_buf env, int val)
siglongjmp:
	;get old value of savemask from env[0] - figure out if we'll restore signals
	mov RAX, [RDI]
	cmp RAX, 0
	je .nosigs
		
		push RDI ;Preserve across call to _sc_sigmask
		push RSI
		
		extern _sc_sig_mask ;System call to alter/get signal mask
		mov RSI, [RDI + 8] ;Mask preserved in env[1]
		mov RDI, 2 ;SIG_SETMASK
		call _sc_sig_mask
		
		pop RSI
		pop RDI
	.nosigs:
	
	;Do the rest of the jump from env[2...]
	add RDI, 16
	jmp _longjmp
	

;TLS and spinlock implementation
global _tls
_tls:
	rdgsbase RAX
	ret

global _spl_lock
_spl_lock:
	mov AX, 0x0100 ;AH = 1, AL = 0
	lock cmpxchg [RDI], AH
	jnz .spin
		mfence
		ret
	.spin:
		pause
		mov AL, [RDI]
		cmp AL, 0
		je _spl_lock
	jmp .spin

global _spl_unlock
_spl_unlock:	
	mfence
	mov [RDI], byte 0
	ret

	
bits 64
section .bss
	
;Space for stack
alignb 4096
_crt_stack:
	resb 4096 * 8
	.top:	

;Space for signal-handling stack
alignb 4096
_crt_sigstack:
	resb 4096 * 8
	.top:
	
bits 64
section .data
	
;Space for initial thread TLS
align 4096
_crt_tls0:
	dd _crt_tls0 ;TLS starts with pointer to itself
	times (4096 - 8) db 0
	.top:
	
