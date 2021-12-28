;cpuinit.asm
;CPU setup and tables for AMD64
;Bryan E. Topp <betopp@betopp.com> 2021

section .text
bits 32

;Macro that makes a physical address from a symbol address.
%define PHYS(symbol) (symbol - 0xFFFFFFFF80000000)

;Bounds of kernel from linker
extern _MULTIBOOT_LOAD_ADDR
extern _MULTIBOOT_BSS_END_ADDR

;How many pagetables we initially allocate for the kernel
%define CPUINIT_STARTPT 8

;Where we copy the SMP trampoline
%define CPUINIT_SMPSTUB_ADDR 0x4000

;How many CPUs we support (xAPIC for now, 8-bit IDs)
%define CPUINIT_CPU_MAX 256

;Size of task-state segment per CPU
%define CPUINIT_TSS_SIZE 128
%define CPUINIT_TSS_SIZE_LOG2 7

;Size of stack per CPU
%define CPUINIT_STACK_SIZE 4096
%define CPUINIT_STACK_SIZE_LOG2 12

;Entered from bootloader in 32-bit protected mode without paging.
global cpuinit_entry
cpuinit_entry:

	;Build page tables to map the kernel.
	;This depends heavily on the difference between kernel physical and virtual addresses.
	;See: PHYS(symbol) defined above, and _KERNEL_VOFFS in linker script.
	
	;Kernel has one PML4, of course. Initially, set it up with identity-mapped and proper virtual addresses.
	
	;Make sure we've got enough pagetables to cover the kernel as linked.
	mov ECX, _MULTIBOOT_BSS_END_ADDR
	shr ECX, 12
	cmp ECX, (CPUINIT_STARTPT * 512)
	jae cpuinit_entry
	
	;Top entry of PML4 references the one PDPT, representing top 512GBytes.
	mov EDI, PHYS(cpuinit_pml4)
	mov EAX, PHYS(cpuinit_pdpt)
	or EAX, 0x3 ;Present, writable
	mov [EDI + (511*8)], EAX ;Top entry, i.e. top 512GByte
	mov [EDI + (  0*8)], EAX ;Bottom entry, i.e. identity-map
	
	;Second-to-top entry of PDPT, representing one PD, representing second-to-top 1GByte.
	mov EDI, PHYS(cpuinit_pdpt)
	mov EAX, PHYS(cpuinit_pd)
	or EAX, 0x3 ;Present, writable
	mov [EDI + (510*8)], EAX ;Second-to-top entry, i.e. -2GByte
	mov [EDI + (  0*8)], EAX ;Bottom entry, i.e. identity-map
	
	;Fill PD to point at pagetables - preallocated for the whole 1GByte
	mov EDI, PHYS(cpuinit_pd)
	mov EAX, PHYS(cpuinit_pt)
	or EAX, 0x3 ;Present, writable
	mov ECX, CPUINIT_STARTPT ;How many PDEs to make - i.e. how many PTs to reference
	.pd_loop:
		mov [EDI], EAX
		add EDI, 8 ;Next 8-byte page directory entry
		add EAX, 4096 ;Next page table
		loop .pd_loop
	.pd_done:
	
	;Fill pagetable entries to cover kernel as-linked
	mov ECX, _MULTIBOOT_BSS_END_ADDR
	sub ECX, _MULTIBOOT_LOAD_ADDR
	shr ECX, 12 ;Number of pages needed to map the kernel
	mov EDI, _MULTIBOOT_LOAD_ADDR ;First address we need to cover
	shr EDI, 12 ;First entry in pagetable to fill
	shl EDI, 3 ;Each entry is 8 bytes
	add EDI, PHYS(cpuinit_pt) ;Location in pagetable
	mov EAX, _MULTIBOOT_LOAD_ADDR ;Where it refers to
	or EAX, 0x3 ;Present, writable
	.pt_loop:
		mov [EDI], EAX
		add EDI, 8 ;Next 8-byte pagetable entry
		add EAX, 4096 ;Next page
		loop .pt_loop
	.pt_done:
	
	;Make the second-to-top PML4e reference a second PDPT, where we'll map physical space directly
	mov EDI, PHYS(cpuinit_pml4)
	mov EAX, PHYS(cpuinit_pdpt_pspace)
	or EAX, 0x3
	mov [EDI + (510*8)], EAX ;Second-to-top entry, second-highest 512GByte
	
	;Fill that PDPT with 1GByte pages, direct-mapping physical space
	mov EAX, 0x83 ;Present, writable, last-level, 0 address
	mov EDX, 0
	mov EDI, PHYS(cpuinit_pdpt_pspace)
	mov ECX, 512
	.pspace_loop:
		mov [EDI + 0], EAX
		mov [EDI + 4], EDX
		add EDI, 8 ;Next PDPTe
		add EAX, 4096 * 512 * 512 ;Next 1GByte hugepage
		adc EDX, 0 ;Need to use 64-bit addresses
		loop .pspace_loop
	.pspace_done:
	
	
	;Before we actually go into paged mode, and need to virtually map stuff for this to work, get the other cores running.

	;Copy our trampoline to conventional memory for the other cores
	mov ESI, PHYS(cpuinit_smpstub)
	mov EDI, CPUINIT_SMPSTUB_ADDR
	mov ECX, cpuinit_smpstub.end - cpuinit_smpstub
	rep movsb

	;Get APIC base address
	mov ECX, 0x1B ;APIC BAR MSR
	rdmsr
	mov EBX, EAX
	and EBX, 0xFFFFF000
	
	;Clear APIC errors
	mov EAX, 0
	mov [EBX + 0x280], EAX
	
	;Send an INIT IPI to all remote cores
	mov EAX, 0xC4500 ;Init IPI, positive edge-trigger, to all-except-self
	mov [EBX + 0x300], EAX
	
	;Wait for delivery
	.init_wait:
	pause
	nop
	mov EAX, [EBX + 0x300]
	and EAX, 1<<12
	jnz .init_wait
	
	;Send a STARTUP IPI to all remote cores.
	mov EAX, 0xC4600 ;Startup IPI, positive edge-trigger, to all-except-self
	or EAX, 4 ;Page number to start executing
	mov [EBX + 0x300], EAX
	
	;Wait for delivery
	.sipi_wait:
	pause
	nop
	mov EAX, [EBX + 0x300]
	and EAX, 1<<12
	jnz .sipi_wait
	
	;Wait a moment before sending a second STARTUP IPI
	mov ECX, 65535
	.second_sipi_delay:
	loop .second_sipi_delay
	mov [EBX + 0x300], EAX
	.apic_done:
	
	;Other cores will get themselves into 32-bit protected mode and jump here.
cpuinit_entry_all:

	;All cores run through this path.

	;Turn on PAE (8-byte pagetable entries) and FSGSBASE (rdgsbase/wrgsbase instructions)
	mov EAX, CR4
	or EAX, 1<<5 ;PAE
	or EAX, 1<<16 ;FSGSBASE
	mov CR4, EAX
	
	;Turn on Long Mode Enable (so Paging will trigger Long Mode Active) and NX-bit support
	mov ECX, 0xC0000080 ;EFER
	rdmsr
	or EAX, 1<<8 ;LME
	or EAX, 1<<11 ;NX
	wrmsr
	
	;Use our newly-built PML4 when we turn on paging
	mov EAX, PHYS(cpuinit_pml4)
	mov CR3, EAX
	
	;Enable paging, kicking us into long-mode.
	mov EAX, CR0
	or EAX, 1<<31 ;PG
	mov CR0, EAX
	
	;Our kernel is now accessible at its proper virtual addresses.
	;But we can't get there yet, because we're only using 32-bit addressing.
	;Set up a Global Descriptor Table and a 64-bit Code Segment Descriptor and jump into it.
	mov EAX, PHYS(.tempgdtr64)
	lgdt [EAX]
	jmp (8) : PHYS(.target64)
	
	.tempgdt64:
		dq 0 ;Null
		db 0, 0, 0, 0, 0, 0x98, 0x20, 0 ;64-bit code			
	.tempgdtr64:
		dw 15 ;Limit
		dd PHYS(.tempgdt64) ;Base
	
	.target64:
	bits 64
	
	;Now we're still in identity-mapped space, but using 64-bit addressing.
	;Jump into virtual space.
	mov RAX, .targetvirt
	jmp RAX
	.targetvirt:
	
	;Alright, we're in virtual kernel space now.
	
	;Use the proper descriptor tables.
	lgdt [cpuinit_gdtr]
	lidt [cpuinit_idtr]
	
	;Use proper data segment descriptors.
	mov AX, cpuinit_gdt.r0data64 - cpuinit_gdt
	mov SS, AX
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;Turn on SYSCALL/SYSRET and set up entry points
	mov RCX, 0xC0000080 ;EFER
	rdmsr
	or RAX, (1<<0) ;SCE
	wrmsr
	
	mov RCX, 0xC0000084 ;SFMASK
	rdmsr
	mov RAX, 0xFFFFFFFF ;Clear all flags on SYSCALL
	wrmsr
	
	mov RCX, 0xC0000083 ;CSTAR - target RIP for compatibility-mode syscalls
	mov RAX, cpuinit_syscall_32
	mov RDX, RAX
	shr RDX, 32
	wrmsr
	
	mov RCX, 0xC0000082 ;LSTAR - target RIP for 64-bit-mode syscalls
	mov RAX, cpuinit_syscall_64
	mov RDX, RAX
	shr RDX, 32
	wrmsr
	
	mov RCX, 0xC0000081 ;STAR
	mov RDX, (cpuinit_gdt.r3dummy - cpuinit_gdt) | 0x3 ;SYSRET CS and SS (+16 and +8 respectively)
	shl RDX, 16
	or RDX, (cpuinit_gdt.r0code64 - cpuinit_gdt) | 0x0 ;SYSCALL CS and SS (+0 and +8 respectively)
	mov RAX, 0 ;32-bit SYSCALL target (unused)
	wrmsr
	
	;Figure out what core ID we are.
	;We could pull this from the APIC or whatever but this is easier.
	mov RAX, 0 ;Compare with the existing variable
	mov RCX, 1 ;Try to replace with this
	.coreloop:
		lock cmpxchg [cpuinit_ncores], RCX
		jz .coredone
		mov RAX, RCX
		inc RCX
		jmp .coreloop
	.coredone:
	
	;Now RAX contains our core number. Set aside in RBX.
	mov RBX, RAX
	
	;Build a task-state segment descriptor, in the appropriate slot in the TSS descriptor array.
	
	;We store the descriptor in the Global Descriptor Table's TSS Descriptor array
	mov RDI, RBX ;Our core number determines which TSS descriptor we use
	shl RDI, 4 ;Each TSS descriptor takes 16 bytes
	add RDI, cpuinit_gdt.ktss_array ;Find location in TSS descriptor array
	
	;We'll point the TSS descriptor at storage in our TSS array.
	mov RSI, RBX ;Our core number determines where our TSS is stored
	shl RSI, CPUINIT_TSS_SIZE_LOG2 ;Offset within array
	add RSI, cpuinit_ktss_storage ;Find location we'll use
	
	;Build the descriptor...
	mov RAX, CPUINIT_TSS_SIZE - 1
	mov [RDI + 0], AX ;Segment limit 15..0
	
	mov RAX, RSI
	mov [RDI + 2], AX ;Base address 15..0
	shr RAX, 16
	mov [RDI + 4], AL ;Base address 23..16
	mov [RDI + 7], AH ;Base address 31..24
	shr RAX, 16
	mov [RDI + 8], EAX ;Base address 63..32
	
	mov AL, 0b10001001 ;Present, DPL=0, available 64-bit TSS
	mov [RDI + 5], AL
	
	mov AL, 0b00000000 ;1-byte limit granularity, 0000 upper bits of limit
	mov [RDI + 6], AL
	
	mov EAX, 0 ;Upper 4 bytes of descriptor unused
	mov [RDI + 12], EAX
	
	;Select the task-state segment descriptor we just built.
	;In the future we can use the task register to determine which CPU core we're on.
	mov RAX, RBX ;Get core number
	shl RAX, 4 ;Each descriptor is 16 bytes
	add RAX, cpuinit_gdt.ktss_array - cpuinit_gdt ;Make into index in GDT
	ltr AX
	
	;Pick our proper per-CPU stack.
	mov RAX, RBX ;Get core number
	shl RAX, CPUINIT_STACK_SIZE_LOG2 ;Make offset in array of stacks
	add RAX, cpuinit_stack_storage ;Position in array
	add RAX, CPUINIT_STACK_SIZE ;Start at top of stack
	mov RSP, RAX
	
	;Store top-of-stack in our task state segment
	mov RAX, RBX ;Get core number
	shl RAX, CPUINIT_TSS_SIZE_LOG2
	add RAX, cpuinit_ktss_storage
	mov [RAX + 4], RSP
	
	;If we're not core 0, run through kernel setup.
	cmp RBX, 0
	jne .notcore0
	
		;Build interrupt descriptor table
		mov RDI, cpuinit_idt
		mov RSI, cpuinit_isrptrs
		mov RCX, 256 ;Fill all vectors
		.idt_loop:
		
			mov AL, 0x8E
			mov [RDI + 5], AL ;P=1, DPL=00, 0, Type=interrupt gate
		
			mov AL, 0
			mov [RDI + 4], AL ;Interrupt Stack Table number (don't use) 000, ignored 00000		
		
			mov AX, cpuinit_gdt.r0code64 - cpuinit_gdt
			mov [RDI + 2], AX ;Target selector
			
			mov RAX, [RSI]
			mov [RDI + 0], AX ;Target Offset[15:0]
			shr RAX, 16
			mov [RDI + 6], AX ;Target Offset[31:16]
			shr RAX, 16
			mov [RDI + 8], EAX ;Target Offset[63:32]
			
			mov EAX, 0
			mov [RDI + 12], EAX ;Reserved
			
			add RDI, 16 ;16 bytes per interrupt descriptor
			add RSI, 8 ;8 byte addresses of ISRs
			loop .idt_loop ;Count down RCX
		.idt_done:
		
		;Set up interrupt handling
		extern pic8259_init
		call pic8259_init
		
		;Set up keyboard
		extern ps2kbd_init
		call ps2kbd_init
	
		;Set up framebuffer output
		extern m_fb_init
		call m_fb_init
	
		;Set up memory allocator with info from multiboot
		extern m_frame_init
		call m_frame_init
		
		;Do uni-processor kernel setup
		extern entry_boot
		call entry_boot
		
		;Indicate that other cores can proceed
		mov RAX, 1
		mov [cpuinit_kready], RAX
	
	.notcore0:
	
	;Wait for core 0 to complete its kernel setup, then go in to run the kernel
	.waitcore0:
	mov RAX, [cpuinit_kready]
	cmp RAX, 0
	pause
	nop
	je .waitcore0
	
	extern entry_smp
	call entry_smp
	
	;Should never return here. Triple-fault.
	lidt [.crash_idtr]
	int 0xFF
	.spin:
	jmp .spin
	.crash_idtr:
		dw 1 ;Limit
		dq 0 ;Base

;Trampoline copied to conventional memory for starting-up non-bootstrap cores.
;They begin executing this at the address CPUINIT_SMPSTUB_ADDR.
;We need to relocate references manually by (CPUINIT_SMPSTUB_ADDR - cpuinit_smpstub) to deal with that.
cpuinit_smpstub:
	;Entered in conventional memory in real mode.
	bits 16
	
	;Zero all the data segments
	mov AX, 0
	mov SS, AX
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;Take a longjump so we're no longer based at 0400:0000, but 0000:4000 (i.e. zero the CS).
	jmp 0 : .rebasetarget + (CPUINIT_SMPSTUB_ADDR - cpuinit_smpstub)
	.rebasetarget:
	
	;Enter protected mode.
	mov EAX, CR0
	or EAX, 1 ;PE
	mov CR0, EAX
	
	;Enter 32-bit flat-memory mode, but don't turn on paging...
	
	;Load a temporary Global Descriptor Table with a flat 32-bit code segment descriptor
	mov EAX, .tempgdtr + (CPUINIT_SMPSTUB_ADDR - cpuinit_smpstub)
	lgdt [EAX]
	
	;Jump into that flat 32-bit code segment
	jmp (8) : (.target32 + (CPUINIT_SMPSTUB_ADDR - cpuinit_smpstub))
	
	.tempgdt:
		dq 0 ;Null descriptor
		db 0xFF, 0xFF, 0x00, 0x00, 0x00, 0b10011010, 0b11001111, 0x00 ;Ring-0 32-bit code
		db 0xFF, 0xFF, 0x00, 0x00, 0x00, 0b10010010, 0b11001111, 0x00 ;Ring-0 32-bit data
	.tempgdtr:
		dw 23 ;Limit
		dd .tempgdt + (CPUINIT_SMPSTUB_ADDR - cpuinit_smpstub) ;Base
	
	.target32:
	bits 32
	
	;Load flat 32-bit data segments
	mov AX, 16
	mov SS, AX
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;Follow the same setup path as the bootstrap core once we're in 32-bit protected mode.
	mov EAX, PHYS(cpuinit_entry_all)
	jmp EAX
	.end:
	bits 64
	
;Returns the address of our Task-State Segment in RAX.
global cpuinit_gettss
cpuinit_gettss:
	mov RAX, 0 ;LTR doesn't clear high bits, I think
	str AX ;Get our task register - index of task-state-segment descriptor
	sub RAX, cpuinit_gdt.ktss_array - cpuinit_gdt ;Turn into offset from first TSS descriptor selector
	shr RAX, 4 ;Each TSS descriptor is 16 bytes
	shl RAX, CPUINIT_TSS_SIZE_LOG2 ;Turn into offset in TSS storage
	add RAX, cpuinit_ktss_storage ;Turn into pointer in TSS storage
	ret

;Entered on system-call from 32-bit compatibility-mode (not used)
cpuinit_syscall_32:
	jmp cpuinit_syscall_32
	
;Entered on system-call from 64-bit code
cpuinit_syscall_64:
	;All flags are cleared on entry (given our SFMASK MSR setting).
	;So interrupts are already disabled at this point.
	
	;The SYSCALL instruction has saved the old RIP and RFLAGS in RCX and R11, respectively.
	;The user has passed parameters to us in RDI, RSI, RDX, R8, and R9 as usual.
	;The parameter that they'd normally put in RCX is passed in R10 instead.
	;They expect RBX, RBP, RSP, and R12-R15 preserved, as usual.
	;So we can only clobber RAX at this point.
	
	;We need to get back to the kernel stack.
	;Retrieve the one that we saved in this core's task-state segment.
	mov RAX, 0
	str AX ;Get this core's TSS descriptor selector
	sub RAX, (cpuinit_gdt.ktss_array - cpuinit_gdt) ;Make relative to first TSS descriptor
	shr RAX, 4 ;16 bytes per TSS descriptor - turn into index in the descriptor array
	shl RAX, CPUINIT_TSS_SIZE_LOG2 ;Make offset in TSS storage array
	add RAX, cpuinit_ktss_storage ;Make pointer to this CPU's TSS
	mov RAX, [RAX + 4] ;Load the saved RSP0 from when we last dropped to user-mode.	(Yes, it's a 64-bit value at offset +4.)
	
	;Swap over to the kernel stack, preserving the user's then in RAX.
	xchg RAX, RSP
	
	;Put the return-address and stuff on the stack like taking an interrupt
	push qword 0 ;SS (we don't bother saving this)
	push RAX ;RSP
	push R11 ;RFLAGS
	push qword 0 ;CS (we don't bother saving this)
	push RCX ;RIP
	
	;Put the rest of the user's context, at entry, on the stack
	extern m_drop_putstack
	call m_drop_putstack
	sub RSP, 16
	sub RSP, [RSP]
	
	;Swap back to kernel GS
	swapgs
	
	;Restore the fourth parameter in its usual place, per calling convention
	mov RCX, R10
	
	;Call the kernel to handle the system-call.
	push RSP ;With last parameter as saved context
	extern entry_syscall
	call entry_syscall

bits 64

;Macro for saving temporary registers on entry to external interrupt handler.
;We assume the kernel calling convention will preserve registers that are supposed to be preserved.
;The usually-clobbered registers, though, need us to preserve them on entry/exit from ISR.
%macro irq_save 0
	push RAX
	push RCX
	push RDX
	push RSI
	push RDI
	push R8
	push R9
	push R10
	push R11
%endmacro

%macro irq_restore 0
	pop R11
	pop R10
	pop R9
	pop R8
	pop RDI
	pop RSI
	pop RDX
	pop RCX
	pop RAX
%endmacro



;Entered on any unused interrupt vector
cpuinit_isr_bad:
	.spin:
	jmp .spin

;CPU exceptions follow

;ISR - divide-by-zero error
cpuinit_isr_de:
	.spin:
	jmp .spin
	
;ISR - debug exception
cpuinit_isr_db:
	.spin:
	jmp .spin
	
;ISR - non-maskable interrupt
cpuinit_isr_nmi:
	.spin:
	jmp .spin

;ISR - breakpoint exception
cpuinit_isr_bp:
	.spin:
	jmp .spin

;ISR - overflow exception
cpuinit_isr_of:
	.spin:
	jmp .spin

;ISR - bound-range exception
cpuinit_isr_br:
	.spin:
	jmp .spin

;ISR - undefined opcode exception
cpuinit_isr_ud:
	.spin:
	jmp .spin

;ISR - device-not-available exception
cpuinit_isr_nm:
	.spin:
	jmp .spin

;ISR - double-fault
cpuinit_isr_df:
	.spin:
	jmp .spin

;ISR - coprocessor segment overrun
cpuinit_isr_co:
	.spin:
	jmp .spin

;ISR - invalid TSS exception
cpuinit_isr_ts:
	.spin:
	jmp .spin

;ISR - segment-not-present exception
cpuinit_isr_np:
	.spin:
	jmp .spin

;ISR - stack exception
cpuinit_isr_ss:
	.spin:
	jmp .spin

;ISR - general protection exception
cpuinit_isr_gp:
	.spin:
	jmp .spin

;ISR - page fault
cpuinit_isr_pf:
	.spin:
	jmp .spin

;ISR - x87 floating-point exception pending
cpuinit_isr_mf:
	.spin:
	jmp .spin

;ISR - alignment check exception
cpuinit_isr_ac:
	.spin:
	jmp .spin

;ISR - machine-check exception
cpuinit_isr_mc:
	.spin:
	jmp .spin

;ISR - SIMD floating-point exception
cpuinit_isr_xf:
	.spin:
	jmp .spin

;ISR - hypervisor injection exception
cpuinit_isr_hv:
	.spin:
	jmp .spin

;ISR - VMM communication exception
cpuinit_isr_vc:
	.spin:
	jmp .spin

;ISR - security exception
cpuinit_isr_sx:
	.spin:
	jmp .spin
	
;PIC interrupts follow - must send EOI before returning, to tell PIC that interrupt is done
extern pic8259_pre_iret
	
;ISR - handles legacy IRQ0
cpuinit_isr_irq0:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ1
cpuinit_isr_irq1:
	irq_save
	
	extern ps2kbd_isr
	call ps2kbd_isr
	
	mov RDI, 1
	call pic8259_pre_iret
	
	irq_restore
	iretq

	.spin:
	jmp .spin

;ISR - handles legacy IRQ2
cpuinit_isr_irq2:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ3
cpuinit_isr_irq3:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ4
cpuinit_isr_irq4:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ5
cpuinit_isr_irq5:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ6
cpuinit_isr_irq6:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ7
cpuinit_isr_irq7:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ8
cpuinit_isr_irq8:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ9
cpuinit_isr_irq9:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ10
cpuinit_isr_irq10:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ11
cpuinit_isr_irq11:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ12
cpuinit_isr_irq12:
	.spin:
	jmp .spin
	
;ISR - handles legacy IRQ13
cpuinit_isr_irq13:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ14
cpuinit_isr_irq14:
	.spin:
	jmp .spin

;ISR - handles legacy IRQ15
cpuinit_isr_irq15:
	.spin:
	jmp .spin

;ISR - does nothing but returns, to take the CPU out of halt.
cpuinit_isr_woke:
	iret
	
section .data
bits 64

;Table of interrupt service routines - just the addresses, so we can swizzle them at runtime.
align 8
cpuinit_isrptrs:
	;First 32 - AMD64 CPU exceptions, 0x00...0x1F
	dq cpuinit_isr_de  ; 0
	dq cpuinit_isr_db  ; 1
	dq cpuinit_isr_nmi ; 2
	dq cpuinit_isr_bp  ; 3
	dq cpuinit_isr_of  ; 4
	dq cpuinit_isr_br  ; 5
	dq cpuinit_isr_ud  ; 6
	dq cpuinit_isr_nm  ; 7
	dq cpuinit_isr_df  ; 8
	dq cpuinit_isr_co  ; 9
	dq cpuinit_isr_ts  ;10
	dq cpuinit_isr_np  ;11
	dq cpuinit_isr_ss  ;12
	dq cpuinit_isr_gp  ;13
	dq cpuinit_isr_pf  ;14
	dq cpuinit_isr_bad ;15
	dq cpuinit_isr_mf  ;16
	dq cpuinit_isr_ac  ;17
	dq cpuinit_isr_mc  ;18
	dq cpuinit_isr_xf  ;19
	dq cpuinit_isr_bad ;20
	dq cpuinit_isr_bad ;21
	dq cpuinit_isr_bad ;22
	dq cpuinit_isr_bad ;23
	dq cpuinit_isr_bad ;24
	dq cpuinit_isr_bad ;25
	dq cpuinit_isr_bad ;26
	dq cpuinit_isr_bad ;27
	dq cpuinit_isr_hv  ;28
	dq cpuinit_isr_vc  ;29
	dq cpuinit_isr_sx  ;30
	dq cpuinit_isr_bad ;31
	
	;Next 16 - IRQs coming from 8259 (legacy) interrupt controller - 0x20...0x2F
	dq cpuinit_isr_irq0  ;32
	dq cpuinit_isr_irq1  ;33
	dq cpuinit_isr_irq2  ;34
	dq cpuinit_isr_irq3  ;35
	dq cpuinit_isr_irq4  ;36
	dq cpuinit_isr_irq5  ;37
	dq cpuinit_isr_irq6  ;38
	dq cpuinit_isr_irq7  ;39
	dq cpuinit_isr_irq8  ;40
	dq cpuinit_isr_irq9  ;41
	dq cpuinit_isr_irq10 ;42
	dq cpuinit_isr_irq11 ;43
	dq cpuinit_isr_irq12 ;44
	dq cpuinit_isr_irq13 ;45
	dq cpuinit_isr_irq14 ;46
	dq cpuinit_isr_irq15 ;47
	
	;Next 16 - unused - 0x30...0x3F
	times 16 dq cpuinit_isr_bad
	
	;Next 64 - unused - 0x40...0x7F
	times 64 dq cpuinit_isr_bad
	
	;Next 127 - unused - 0x80...0xFE
	times 127 dq cpuinit_isr_bad
	
	;Last interrupt - does nothing but brings the CPU out of halt (0xFF)
	dq cpuinit_isr_woke

;Global descriptor table that we use once CPUs are set up.
align 8
global cpuinit_gdt
global cpuinit_gdt.r3code64
global cpuinit_gdt.r3data64
cpuinit_gdt:
	;Null descriptor (CPU requires this to be in index 0)
	dq 0

	;Kernel code segment (64-bit long mode, ring-0)
	.r0code64: db 0, 0, 0, 0, 0, 0x98, 0x20, 0
	
	;Kernel data segment (ring-0)
	;Note that a SYSCALL instruction will load CS and SS with a certain value +0 and +8.
	;So the order of these two descriptors matters - r0data64 must follow r0code64.
	.r0data64: db 0, 0, 0, 0, 0, 0x92, 0, 0

	;Similarly, SYSRET loads CS and SS with a certain value +16 and +8.
	;The descriptor that SYSRET points to, +0, is used for returning to 32-bit code. We don't support that.
	;So we must have r3dummy, r3data64, and then r3code64.
	.r3dummy: dq 0 ;Don't support returning to 32-bit mode
	
	;User data segment (ring-3)
	.r3data64: db 0, 0, 0, 0, 0, 0xF2, 0, 0
	
	;User code segment (64-bit long mode, ring-3)
	.r3code64: db 0, 0, 0, 0, 0, 0xF8, 0x20, 0
	
	;Descriptors for task-state segments for each CPU
	;Built at runtime, so we can swizzle the address bytes
	align 16
	.ktss_array:
	times (16 * CPUINIT_CPU_MAX) db 0 ;Each descriptor is 16 bytes, and we need one per CPU
	.end:

;Pointer to global descriptor table, for LGDT.
cpuinit_gdtr:
	dw (cpuinit_gdt.end - cpuinit_gdt) - 1 ;Limit
	dq cpuinit_gdt ;Base
	
;Pointer to interrupt descriptor table, for LIDT
cpuinit_idtr:
	dw (cpuinit_idt.end - cpuinit_idt) - 1 ;Limit
	dq cpuinit_idt ;Base
	
section .bss
bits 32

;Task state segments for every CPU
alignb 4096
cpuinit_ktss_storage:
	resb CPUINIT_TSS_SIZE * CPUINIT_CPU_MAX
	
;Early stacks for every CPU
alignb 4096
cpuinit_stack_storage:
	resb CPUINIT_STACK_SIZE * CPUINIT_CPU_MAX
	
;Interrupt descriptor table - has to be built at runtime because of the insane address swizzling.
;Ideally the linker would support building this at build-time, but that doesn't work.
alignb 4096
cpuinit_idt:
	resb 16*256 ;256 interrupt vectors, needing 16 bytes each. 
	.end:

;Page-map level 4 for kernel (only one of these, for whole 48-bit virtual space)
alignb 4096
global cpuinit_pml4 ;Referenced by uspc code (makes copy of kernel PML4 for each user PML4)
cpuinit_pml4:
	resb 4096

;Page directory pointer table for kernel - top 512GBytes of everyone's address space.
;This gets referenced from all user PML4s too.
alignb 4096
global cpuinit_pdpt ;Referenced by kspc code (m_kspc_set, m_kspc_get)
cpuinit_pdpt:
	resb 4096

;Page directory pointer table that represents physical space - next-highest 512GBytes of space.
;This gets referenced from all user PML4s too, too.
alignb 4096
cpuinit_pdpt_pspace:
	resb 4096

;Initial page directory for kernel. Second-to-top 1GByte of address space.
alignb 4096
cpuinit_pd:
	resb 4096
	
;Initial pagetables for kernel.
alignb 4096
cpuinit_pt:
	resb 4096*CPUINIT_STARTPT

	
;Number of cores that have started up. Used with an atomic to figure out "which core am I".
alignb 8
cpuinit_ncores:
	resb 8

;Whether core-0 has finished early init
alignb 8
cpuinit_kready:
	resb 8

