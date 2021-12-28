//m_fb.c
//Framebuffer implementation on AMD64
//Bryan E. Topp <betopp@betopp.com> 2021

#include "m_fb.h"
#include "m_panic.h"
#include "amd64.h"

//Register indexes for DISPI
#define DISPI_REG_ID          0x00
#define DISPI_REG_XRES        0x01
#define DISPI_REG_YRES        0x02
#define DISPI_REG_BPP         0x03
#define DISPI_REG_ENABLE      0x04
#define DISPI_REG_BANK        0x05
#define DISPI_REG_VIRT_WIDTH  0x06
#define DISPI_REG_VIRT_HEIGHT 0x07
#define DISPI_REG_X_OFFSET    0x08
#define DISPI_REG_Y_OFFSET    0x09

#define FB_WIDTH 800
#define FB_HEIGHT 600

//Performs a write to the given dispi register.
static void dispi_reg_write(uint16_t idx, uint16_t val)
{
	outw(0x1ce, idx);
	outw(0x1d0, val);
}

//Performs a read from the given dispi register.
static uint16_t dispi_reg_read(uint16_t idx)
{
	outw(0x1ce, idx);
	return inw(0x1d0);
}

void m_fb_init(void)
{
	dispi_reg_write(DISPI_REG_ID, 0xB0C4);
	uint16_t host_dispi_version = dispi_reg_read(DISPI_REG_ID);
	if(host_dispi_version < 0xB0C4)
		m_panic("m_fb_init: no DISPI 0xB0C4");
	
	dispi_reg_write(DISPI_REG_ENABLE, 0);
	dispi_reg_write(DISPI_REG_XRES, FB_WIDTH);
	dispi_reg_write(DISPI_REG_YRES, FB_HEIGHT);
	dispi_reg_write(DISPI_REG_BPP, 32);
	dispi_reg_write(DISPI_REG_ENABLE, 0x41); //0x40 = use LFB	
}

void m_fb_info(uintptr_t *paddr_out, int *w_out, int *h_out, size_t *stride_bytes_out)
{
	//Hardcoded whatever
	*paddr_out = 0xFD000000;
	*w_out = FB_WIDTH;
	*h_out = FB_HEIGHT;
	*stride_bytes_out = FB_WIDTH * 4;
}
