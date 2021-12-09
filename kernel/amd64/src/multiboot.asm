;multiboot.asm
;Entry from bootloader
;Bryan E. Topp <betopp@betopp.com> 2021

section .multiboot
bits 32

;Macro that makes a physical address from a symbol address.
%define PHYS(symbol) (symbol - 0xFFFFFFFF80000000)

;Bootloader looks for this to find header
%define MULTIBOOT_MAGIC 0x1BADB002

;Flags provided to bootloader.
;Bit 16 - we have memory info.
%define MULTIBOOT_FLAGS 0x00010000

;Header checksum
%define MULTIBOOT_CHECKSUM (0x100000000 - MULTIBOOT_MAGIC - MULTIBOOT_FLAGS)

;Physical addresses defined in linker script
extern _MULTIBOOT_LOAD_ADDR
extern _MULTIBOOT_LOAD_END_ADDR
extern _MULTIBOOT_BSS_END_ADDR

;Size of storage for multiboot's memory map
%define MULTIBOOT_MMAP_MAX 1024

;Multiboot header, identifying us to the bootloader
global multiboot_header
multiboot_header:
	dd MULTIBOOT_MAGIC
	dd MULTIBOOT_FLAGS
	dd MULTIBOOT_CHECKSUM
	dd (multiboot_header - $$) + _MULTIBOOT_LOAD_ADDR ;Where this header goes
	dd _MULTIBOOT_LOAD_ADDR ;Start-of-loading
	dd _MULTIBOOT_LOAD_END_ADDR ;End-of-loading
	dd _MULTIBOOT_BSS_END_ADDR ;End-of-kernel
	dd (multiboot_entry - $$) + _MULTIBOOT_LOAD_ADDR ;Entry point
	
;Entry point from bootloader - 32-bit protected mode, but with paging off
global multiboot_entry
multiboot_entry:
	
	;Make sure we were loaded by multiboot - crash otherwise
	cmp EAX, 0x2BADB002
	jne multiboot_entry
	
	;Mask all 8259 PIC interrupts before starting - we don't use the 8259.
	mov AL, 0xFF
	out 0x21, AL
	out 0xA1, AL
	
	;Get rid of EGA cursor
	mov AL, 0x0A
	mov DX, 0x3D4
	out DX, AL
	mov AL, 0x20
	inc DX
	out DX, AL
	
	;Look up the flags from the multiboot information structure.
	mov EAX, [EBX + 0]
	
	;Make sure multiboot gave us memory information
	and EAX, 1<<6
	cmp EAX, 1<<6
	jne multiboot_entry
	
	;Get memory-map location and length
	mov ESI, [EBX + 48]
	mov ECX, [EBX + 44]
	
	;Cap length to the amount of space we reserve
	cmp ECX, 0
	je .mmap_done
	cmp ECX, MULTIBOOT_MMAP_MAX
	jb .mmap_fits
		mov ECX, MULTIBOOT_MMAP_MAX
	.mmap_fits:
	
	mov [PHYS(multiboot_mmap_size)], ECX
	
	;Copy the memory-map to our own storage for it
	mov EDI, PHYS(multiboot_mmap_storage)
	rep movsb
	
	.mmap_done:
	
	;Check that we have the CPU features we want
	
	mov EAX, 0x1
	cpuid
	
	test EDX, (1<<6) ;PAE
	jnz .have_pae
		mov ESI, PHYS(.no_pae)
		jmp multiboot_failure
		.no_pae: db "!PAE", 0
	.have_pae:
	
	test EDX, (1<<9) ;APIC
	jnz .have_apic
		mov ESI, PHYS(.no_apic)
		jmp multiboot_failure
		.no_apic: db "!APIC", 0
	.have_apic:
	
	mov EAX, 0x80000001
	cpuid
	
	test EDX, (1<<29) ;LM
	jnz .have_lm
		mov ESI, PHYS(.no_lm)
		jmp multiboot_failure
		.no_lm: db "!LM", 0
	.have_lm:
	
	test EDX, (1<<20) ;NX
	jnz .have_nx
		mov ESI, PHYS(.no_nx)
		jmp multiboot_failure
		.no_nx: db "!NX", 0
	.have_nx:
	
	test EDX, (1<<11) ;SYSCALL
	jnz .have_syscall
		mov ESI, PHYS(.no_syscall)
		jmp multiboot_failure
		.no_syscall: db "!SYSCALL", 0
	.have_syscall:
	
	mov EAX, 0x7
	mov ECX, 0x0
	cpuid
	
	test EBX, (1<<0) ;FSGSBASE
	jnz .have_fsgsbase
		mov ESI, PHYS(.no_fsgsbase)
		jmp multiboot_failure
		.no_fsgsbase: db "!FSGSBASE", 0
	.have_fsgsbase:
	
	;Head into setting up our AMD64
	extern cpuinit_entry
	jmp cpuinit_entry


;Failure reporting from multiboot.
;ESI should contain string.
multiboot_failure:
	mov EDI, 0xB8000
	mov EAX, 0
	mov ECX, 80*25
	rep stosw
	
	mov EDI, 0xB8000
	mov EDX, PHYS(.prefixstr)
	mov ECX, 15
	.prefixloop:
		mov AL, [EDX]
		mov [EDI], AL
		inc EDI
		mov [EDI], byte 0xC0
		inc EDI
		inc EDX
		loop .prefixloop
	
	.loop:
		mov AL, [ESI]
		cmp AL, 0
		je .done
		mov [EDI], AL
		inc EDI
		mov [EDI], byte 0xC0
		inc EDI
		inc ESI
		jmp .loop
	.done:
	hlt
	jmp .done

	.prefixstr: db "(Stump) CPUID: "

section .bss

;Storage for multiboot's memory map, provided by BIOS
global multiboot_mmap_storage
multiboot_mmap_storage:
	resb MULTIBOOT_MMAP_MAX

global multiboot_mmap_size
multiboot_mmap_size:
	resb 8
