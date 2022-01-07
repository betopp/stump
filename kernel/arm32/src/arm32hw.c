//arm32hw.c
//ARM 32-bit platform hardware - as emulated by QEMU for now.
//Bryan E. Topp <betopp@betopp.com> 2021

#include "m_fb.h"
#include "m_kspc.h"
#include "m_frame.h"

#include <stdint.h>

#include "pl110.h"
#include "pl050.h"
#include "pic.h"
#include "ps2kbd.h"

//Called on boot, as soon as virtual memory is ready
void _hw_init(void)
{
	//Set up free memory...
	size_t framesize = m_frame_size();
	
	//Defined in linker script
	extern uint8_t _KSPACE_BASE[];
	extern uint8_t _KERNEL_END[];
	extern uint8_t _PMEM_END[];
	
	//Allocatable range starts after kernel, ends at end of memory
	uintptr_t alloc_start = (uintptr_t)(_KERNEL_END) - (uintptr_t)(_KSPACE_BASE);
	uintptr_t alloc_end = (uintptr_t)(_PMEM_END);
	for(uintptr_t ff = alloc_start; ff < alloc_end; ff += framesize)
	{
		m_frame_free(ff);
	}
	
	//Init hardware
	pl050_init(); //PS/2 interfaces
	pl110_init(); //framebuffer
	pic_init(); //interrupt controller
	ps2kbd_init();
}

//Called on fast interrupt request
void _hw_fiq(void)
{
	pic_fiq();
}

//Called on interrupt request
void _hw_irq(void)
{
	pic_irq();
}
