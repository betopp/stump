//fb.c
//Framebuffer handling in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "fb.h"
#include "kpage.h"
#include "m_panic.h"
#include "m_fb.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "logo.xbm"

//Framebuffer information from machine-specific code
static int fb_width;
static int fb_height;
static size_t fb_stride;

//Space where we map the framebuffer in the kernel
static uint32_t *fb_ptr;

void fb_init(void)
{
	//Find framebuffer hardware info
	uintptr_t paddr;
	m_fb_info(&paddr, &fb_width, &fb_height, &fb_stride);
	
	//Map framebuffer in kernel-space
	fb_ptr = kpage_physadd(paddr, fb_height * fb_stride);
	if(fb_ptr == NULL)
		m_panic("fb_init failed to map fb");
	
	//Clear it
	for(int yy = 0; yy < fb_height; yy++)
	{
		uint32_t *line = (uint32_t*)(((char*)fb_ptr) + (yy * fb_stride));
		for(int xx = 0; xx < fb_width; xx++)
		{
			line[xx] = 0;
		}
	}
	
	//Draw logo
	const uint8_t *logo_ptr = logo_bits;
	for(int yy = (fb_height - logo_height) / 2; yy < (fb_height + logo_height) / 2; yy++)
	{
		uint32_t *line = (uint32_t*)(((char*)fb_ptr) + (yy * fb_stride));
		line += (fb_width - logo_width) / 2;
		for(int xx = 0; xx < logo_width; xx++)
		{
			if(logo_ptr[xx / 8] & (1 << (xx%8)))
				line[xx] = 0xFFFFFFFF;
		}
		logo_ptr += (logo_width + 7) / 8;
	}
}


void fb_paint(const fb_back_t *back)
{
	if(back->width == fb_width && back->height == fb_height && back->stride == fb_stride)
	{
		//Easy case, worth special-casing
		memcpy(fb_ptr, back->bufptr, fb_height * fb_stride);
		return;
	}
	
	//Dumb approach for now because I don't feel like working through Bresenham spans
	//Todo - maybe the machine-layer has some kinda 2D GPU abstraction or whatever. I'll allow this one case.
	for(int yy = 0; yy < fb_height; yy++)
	{
		int ys = (yy * back->height) / fb_height;
		const uint32_t *line_src = (const uint32_t*)(((const uint8_t*)(back->bufptr)) + (ys * back->stride));
		uint32_t *line_dst = (uint32_t*)(((uint8_t*)(fb_ptr)) + (yy * fb_stride));
		
		for(int xx = 0; xx < fb_width; xx++)
		{
			int xs = (xx * back->width) / fb_width;
			line_dst[xx] = line_src[xs];
		}
	}
		
}