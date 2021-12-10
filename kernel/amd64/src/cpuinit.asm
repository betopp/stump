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

	;Turn on PAE (8-byte pagetable entries)
	mov EAX, CR4
	or EAX, 1<<5 ;PAE
	mov CR4, EAX
	
	;Turn on Long Mode Enable (so Paging will trigger Long Mode Active)
	mov ECX, 0xC0000080 ;EFER
	rdmsr
	or EAX, 1<<8 ;LME
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
	
	;If we're not core 0, run through kernel setup.
	cmp RBX, 0
	jne .notcore0
	
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
	
	;Load data segments
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
	ltr AX ;Get our task register - index of task-state-segment descriptor
	sub RAX, cpuinit_gdt.ktss_array - cpuinit_gdt ;Turn into offset from first TSS descriptor selector
	shr RAX, 4 ;Each TSS descriptor is 16 bytes
	shl RAX, CPUINIT_TSS_SIZE_LOG2 ;Turn into offset in TSS storage
	add RAX, cpuinit_ktss_storage ;Turn into pointer in TSS storage
	ret

	
section .data
bits 64

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

