//m_frame.h
//Physical memory allocator, per-machine
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_FRAME_H
#define M_FRAME_H

#include <sys/types.h>
#include <stdint.h>

//Returns the size of physical frame that this machine allocates.
size_t m_frame_size(void);

//Allocates a frame and returns its physical address.
uintptr_t m_frame_alloc(void);

//Frees a frame, returning it to the frames available to allocate.
void m_frame_free(uintptr_t frame);

#endif //M_FRAME_H

