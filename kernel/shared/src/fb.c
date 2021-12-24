//fb.c
//Framebuffer handling in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "fb.h"
#include "kpage.h"
#include "m_panic.h"
#include "m_fb.h"
#include <stddef.h>
#include <stdint.h>
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
	// 'ham spanna
	//todo - options for scaling
	
	//Source line/pixel always repeated this many times
	int y_whole = fb_height / back->height;
	int x_whole = fb_width / back->width;
	
	//How frequently an extra repetition happens
	int y_partial = 0;
	if(fb_height % back->height != 0)
		y_partial = back->height / (fb_height - (y_whole * back->height));
	
	int x_partial = 0;
	if(fb_width % back->width != 0)
		x_partial = back->width / (fb_width - (x_whole * back->width));
	
	int y_remain = y_partial;
	int y_src = 0;
	int y_dst = 0;
	const uint32_t *y_src_ptr = back->bufptr;
	uint32_t *y_dst_ptr = fb_ptr;
	while(y_dst < fb_height)
	{
		//Always y_whole repetitions of the source line - sometimes y_whole+1
		int y_reps = y_whole;
		y_remain--;
		if(y_remain == 0)
		{
			y_reps++;
			y_remain = y_partial;
		}
		
		for(int yy = 0; yy < y_reps; yy++)
		{
			//Paint the line
			int x_remain = x_partial;
			int x_src = 0;
			int x_dst = 0;			
			const uint32_t *x_src_ptr = y_src_ptr;
			uint32_t *x_dst_ptr = y_dst_ptr;
			while(x_dst < fb_width)
			{
				//Always x_whole repetitions of the source pixel - sometimes x_whole+1
				int x_reps = x_whole;
				x_remain--;
				if(x_remain == 0)
				{
					x_reps++;
					x_remain = x_partial;
				}
				
				for(int xx = 0; xx < x_reps; xx++)
				{
					//Actual pixel copied here
					*x_dst_ptr = *x_src_ptr;
					
					x_dst++;
					x_dst_ptr++;
				}
				x_src++;
				x_src_ptr++;
			}
			y_dst++;
			y_dst_ptr += fb_stride / sizeof(*y_dst_ptr);
		}
		y_src++;
		y_src_ptr += back->stride / sizeof(*y_src_ptr);
	}
		
}