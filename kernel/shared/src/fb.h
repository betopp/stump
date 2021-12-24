//fb.h
//Framebuffer handling in kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef FB_H
#define FB_H

#include <stddef.h>
#include <stdint.h>

//Backbuffer that might be painted into the framebuffer
typedef struct fb_back_s
{
	uint32_t *bufptr; //Kernel buffer for storing the image at original size
	size_t buflen; //Size of kernel buffer in bytes
	int width; //Width in pixels
	int height; //Height in lines
	size_t stride; //Address difference from one line to the next in bytes
} fb_back_t;

//Initializes framebuffer handling.
void fb_init(void);

//Paints a new frame into the framebuffer.
void fb_paint(const fb_back_t *back);

#endif //FB_H

