//pic.c
//Interrupt controller on QEMU ARM board
//Bryan E. Topp <betopp@betopp.com> 2021

#include "pic.h"
#include "pl050.h"
#include <stdint.h>
#include <stddef.h>
#include "m_kspc.h"

typedef struct _pic_regs_s
{
	uint32_t PIC_IRQ_STATUS; //IRQ Status register
	uint32_t PIC_IRQ_RAWSTAT; //Raw interrupt status
	uint32_t PIC_IRQ_ENABLESET; //Interrupt enable register
	uint32_t PIC_IRQ_ENABLECLR; //Interrupt clear register
	uint32_t PIC_INT_SOFTSET; //Software interrupt register
	uint32_t PIC_INT_SOFTCLR; //Software interrupt clear register
	uint32_t unused_018;
	uint32_t unused_01c;
	uint32_t PIC_FIQ_STATUS; //FIQ Status register
	uint32_t PIC_FIQ_RAWSTAT; //Raw FIQ status
	uint32_t PIC_FIQ_ENABLESET; //FIQ enable register
	uint32_t PIC_FIQ_ENABLECLR; //FIQ clear register
	

} _pic_regs_t;

//Virtual space reserved for mapping PIC control registers
__attribute__((aligned(4096))) uint32_t _pic_regs_page[1024];

//Macro for accessing pic registers in virtual space
#define pic_REGS ((volatile _pic_regs_t*)(_pic_regs_page))

void pic_init(void)
{
	//Map page to point at pic hardware
	m_kspc_set((uintptr_t)(_pic_regs_page), 0x14000000);
	
	//Enable interrupts we use
	pic_REGS->PIC_IRQ_ENABLESET = (1<<3) | (1<<4);
	
}

void pic_irq(void)
{
	uint32_t status = pic_REGS->PIC_IRQ_STATUS;
	
	if(status & (1<<3)) //Keyboard
		pl050_isr(0);
	
	if(status & (1<<4)) //Mouse
		pl050_isr(1);
}

void pic_fiq(void)
{
	
}

