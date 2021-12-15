;m_drop.asm
;Dropping to usermode from kernel
;Bryan E. Topp <betopp@betopp.com> 2021

section .text
bits 64

;Structure of user context
%define M_DROP_OFF_RAX (8 *  0)
%define M_DROP_OFF_RCX (8 *  1)
%define M_DROP_OFF_RDX (8 *  2)
%define M_DROP_OFF_RBX (8 *  3)
%define M_DROP_OFF_RSP (8 *  4)
%define M_DROP_OFF_RBP (8 *  5)
%define M_DROP_OFF_RSI (8 *  6)
%define M_DROP_OFF_RDI (8 *  7)
%define M_DROP_OFF_R8  (8 *  8)
%define M_DROP_OFF_R9  (8 *  9)
%define M_DROP_OFF_R10 (8 * 10)
%define M_DROP_OFF_R11 (8 * 11)
%define M_DROP_OFF_R12 (8 * 12)
%define M_DROP_OFF_R13 (8 * 13)
%define M_DROP_OFF_R14 (8 * 14)
%define M_DROP_OFF_R15 (8 * 15)
%define M_DROP_OFF_RIP (8 * 16) ;Instruction pointer
%define M_DROP_OFF_FLG (8 * 17) ;Flags
%define M_DROP_OFF_GSB (8 * 18) ;GS-base
%define M_DROP_SIZE    (8 * 19)


;Called on entry to kernel-mode, to preserve user context on kernel stack
global m_drop_putstack
m_drop_putstack:
	;Put size of structure on the stack, for caller to use
	push qword M_DROP_SIZE
	
	;Leave room on stack for saved context
	sub RSP, M_DROP_SIZE
	
	;Store general purpose registers
	mov [RSP + M_DROP_OFF_RAX], RAX
	mov [RSP + M_DROP_OFF_RCX], RCX
	mov [RSP + M_DROP_OFF_RDX], RDX
	mov [RSP + M_DROP_OFF_RBX], RBX
	mov [RSP + M_DROP_OFF_RSP], RSP
	mov [RSP + M_DROP_OFF_RBP], RBP
	mov [RSP + M_DROP_OFF_RSI], RSI
	mov [RSP + M_DROP_OFF_RDI], RDI
	mov [RSP + M_DROP_OFF_R8 ], R8
	mov [RSP + M_DROP_OFF_R9 ], R9
	mov [RSP + M_DROP_OFF_R10], R10
	mov [RSP + M_DROP_OFF_R11], R11
	mov [RSP + M_DROP_OFF_R12], R12
	mov [RSP + M_DROP_OFF_R13], R13
	mov [RSP + M_DROP_OFF_R14], R14
	mov [RSP + M_DROP_OFF_R15], R15
	
	;Get previously-pushed RIP, just before we were called
	mov RAX, [RSP + (M_DROP_SIZE + 16) + (8 * 0)]
	mov [RSP + M_DROP_OFF_RIP], RAX
	
	;Get flags, pushed before RIP and CS (ignored)
	mov RAX, [RSP + (M_DROP_SIZE + 16) + (8 * 2)]
	mov [RSP + M_DROP_OFF_FLG], RAX
	
	;Get RSP, pushed before RIP, CS, and flags
	mov RAX, [RSP + (M_DROP_SIZE + 16) + (8 * 3)]
	mov [RSP + M_DROP_OFF_RSP], RAX
	
	rdgsbase RAX
	mov [RSP + M_DROP_OFF_GSB], RAX
	
	mov RAX, [RSP + M_DROP_OFF_RAX]
	
	add RSP, M_DROP_SIZE
	add RSP, 8
	ret

global m_drop_copy ;void m_drop_copy(m_drop_t *dst, const m_drop_t *src);
m_drop_copy:
	mov RCX, M_DROP_SIZE / 8
	rep movsq
	ret

global m_drop_reset ;void m_drop_reset(m_drop_t *drop, uintptr_t entry);
m_drop_reset:
	;Preserve drop location
	mov RDX, RDI

	;Fill with 0s
	mov RAX, 0
	mov RCX, M_DROP_SIZE / 8
	rep stosq
	
	;Restore drop location
	mov RDI, RDX
	
	;Set entry point
	mov [RDI + M_DROP_OFF_RIP], RSI
	
	ret

global m_drop_retval ;void m_drop_retval(m_drop_t *drop, uintptr_t retval);
m_drop_retval:
	mov [RDI + M_DROP_OFF_RAX], RSI
	ret

global m_drop ;void m_drop(const m_drop_t *drop);
m_drop:

	;Disable interrupts so we don't get caught with the wrong GS-base
	cli
	
	;Set aside kernel GS-base in spare GS-base register
	swapgs
	
	;Set aside the desired RIP, CS, RFLAGS, RSP, SS on our stack.
	;They'll be popped off once we've restored all other registers.
	extern cpuinit_gdt
	extern cpuinit_gdt.r3code64
	extern cpuinit_gdt.r3data64

	;SS
	mov RAX, cpuinit_gdt.r3data64
	sub RAX, cpuinit_gdt
	or RAX, 3
	push RAX
	
	;RSP
	mov RAX, [RDI + M_DROP_OFF_RSP]
	push RAX
	
	;RFLAGS
	mov RAX, [RDI + M_DROP_OFF_FLG]
	push RAX
	
	;CS
	mov RAX, cpuinit_gdt.r3code64
	sub RAX, cpuinit_gdt
	or RAX, 3
	push RAX
	
	;RIP
	mov RAX, [RDI + M_DROP_OFF_RIP]
	push RAX
	
	;Restore all other registers - do RDI last, because it's got our pointer in it	
	mov RAX, [RDI + M_DROP_OFF_GSB]
	wrgsbase RAX

	mov RAX, [RDI + M_DROP_OFF_RAX]
	mov RCX, [RDI + M_DROP_OFF_RCX]
	mov RDX, [RDI + M_DROP_OFF_RDX]
	mov RBX, [RDI + M_DROP_OFF_RBX]
	mov RBP, [RDI + M_DROP_OFF_RBP]
	mov RSI, [RDI + M_DROP_OFF_RSI]
	mov R8,  [RDI + M_DROP_OFF_R8 ]
	mov R9,  [RDI + M_DROP_OFF_R9 ]
	mov R10, [RDI + M_DROP_OFF_R10]
	mov R11, [RDI + M_DROP_OFF_R11]
	mov R12, [RDI + M_DROP_OFF_R12]
	mov R13, [RDI + M_DROP_OFF_R13]
	mov R14, [RDI + M_DROP_OFF_R14]
	mov R15, [RDI + M_DROP_OFF_R15]
	
	mov RDI, [RDI + M_DROP_OFF_RDI]
	
	;Pop RIP, CS, RFLAGS, RSP, SS off our stack, finishing the switch to userspace
	iretq
