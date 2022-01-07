//pl050.c
//ARM PS/2 controller support
//Bryan E. Topp <betopp@betopp.com> 2021

#include "pl050.h"
#include "ps2kbd.h"
#include "m_kspc.h"
#include "m_frame.h"
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>

//Number of PL050 instances
#define PL050_INST_MAX 2

//Base addresses of PL050 instances
static const uintptr_t _pl050_paddrs[PL050_INST_MAX] =
{
	0x18000000,
	0x19000000,
};

//Virtual pages that get remapped to point at PL050 registers
__attribute__((aligned(16384))) uint8_t _pl050_pages[PL050_INST_MAX][16384];

//Register set of PL050
typedef struct _pl050_regs_s
{
	uint32_t KMICR;     //Control register
	uint32_t KMISTAT;   //Status register
	uint32_t KMIDATA;   //Data shift registers (read/write)
	uint32_t KMICLKDIV; //Clock divisor
	uint32_t KMIIR;
	
} _pl050_regs_t;

//Macro for accessing PL050 registers in virtual space
#define PL050_REGS(inst) ((volatile _pl050_regs_t*)(&(_pl050_pages[inst][0])))

//Last received data
int pl050_in_data[PL050_INST_MAX];
int pl050_in_full[PL050_INST_MAX];

void pl050_init(void)
{
	//Remap pages to point at PL050 registers and initialize controllers
	for(int ii = 0; ii < PL050_INST_MAX; ii++)
	{
		m_kspc_set((uintptr_t)(&(_pl050_pages[ii][0])), _pl050_paddrs[ii]);
		
		PL050_REGS(ii)->KMICR = 0x14; //Enable receive interrupt, disable transmit interrupt, enable module
		PL050_REGS(ii)->KMICLKDIV = 0; //For emulation, just assume this doesn't matter
	}	
}

void pl050_isr(int which)
{
	int data = pl050_recv(which);
	if(data >= 0 && which == 0)
	{
		ps2kbd_isr(data & 0xFF);
	}
}

int pl050_send(int which, int ch)
{
	uint32_t stat = PL050_REGS(which)->KMISTAT;
	if(stat & (1<<6)) //TXEMPTY
	{
		PL050_REGS(which)->KMIDATA = (ch & 0xFF);
		return 1;
	}
	else
	{
		return -EAGAIN;
	}
}

int pl050_recv(int which)
{	
	uint32_t stat = PL050_REGS(which)->KMISTAT;
	if(stat & (1<<4)) //RXFULL
		return (PL050_REGS(which)->KMIDATA) & 0xFF;
	else
		return -EAGAIN;
}