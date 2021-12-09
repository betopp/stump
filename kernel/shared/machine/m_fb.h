//m_fb.h
//Framebuffer information, per-machine
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_FB_H
#define M_FB_H

#include <stdint.h>
#include <stddef.h>

//Returns information about the framebuffer.
//Framebuffers are assumed to be uint32_t pixels of 0xAARRGGBB for now.
void m_fb_info(uintptr_t *paddr_out, int *w_out, int *h_out, size_t *stride_bytes_out);

#endif //M_FB_H

