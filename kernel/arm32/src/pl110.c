//pl110.c
//ARM LCD controller support
//Bryan E. Topp <betopp@betopp.com> 2021

#include "pl110.h"
#include "m_fb.h"
#include "m_kspc.h"

//Control registers for versatile/PB display controller
typedef volatile struct _pl110_regs_s
{
	uint32_t LCDTiming0; //00
	uint32_t LCDTiming1; //04
	uint32_t LCDTiming2; //08
	uint32_t LCDTiming3; //0C
	uint32_t LCDUPBASE;  //10
	uint32_t LCDLPBASE;  //14
	uint32_t LCDIMSC;    //18 //transposed according to qemu...?
	uint32_t LCDControl; //1C //transposed according to qemu...?
	uint32_t LCDRIS;     //20
	uint32_t LCDMIS;     //24
	uint32_t LCDICR;     //28
	uint32_t LCDUPCURR;  //2C
	uint32_t LCDLPCURR;  //30
} _pl110_regs_t;

//Virtual space reserved for mapping video control registers
__attribute__((aligned(16384))) uint32_t _pl110_regs_page[4096];
#define PL110_REGS ((volatile _pl110_regs_t*)(_pl110_regs_page))

//Space for framebuffers
__attribute__((aligned(16384))) uint32_t _pl110_fb[480][640];

//Defined in linker script
extern uint8_t _KSPACE_BASE[];

void pl110_init(void)
{
	//Remap video registers page to point at the right physical address
	m_kspc_set((uintptr_t)(_pl110_regs_page), 0xc0000000);

	//Blank the framebuffer
	for(int yy = 0; yy < 480; yy++)
	{
		for(int xx = 0; xx < 640; xx++)
		{
			_pl110_fb[yy][xx] = ((xx+yy)%2) ? 0xFFFFFFFF : 0x0000;
		}
	}

	//Set up video output
	//MMIO for display controller in Integrator/CP PL110
	PL110_REGS->LCDTiming0 = 0x3f1f3f9c;
	PL110_REGS->LCDTiming1 = 0x080b61df;
	PL110_REGS->LCDControl = 0x182b;
	PL110_REGS->LCDUPBASE = (uintptr_t)(&(_pl110_fb[0][0])) - (uintptr_t)(_KSPACE_BASE);
}

void m_fb_info(uintptr_t *paddr_out, int *w_out, int *h_out, size_t *stride_bytes_out)
{
	*paddr_out = (uintptr_t)(&(_pl110_fb[0][0])) - (uintptr_t)(_KSPACE_BASE);
	*w_out = 640;
	*h_out = 480;
	*stride_bytes_out = sizeof(_pl110_fb[0]);
}
