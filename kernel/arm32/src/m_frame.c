//m_frame.c
//Physical memory allocator on 32-bit ARM
//Bryan E. Topp <betopp@betopp.com> 2021

#include "m_frame.h"
#include "m_kspc.h"
#include <string.h>

//Window that we re-map to the head of our free-list.
__attribute__((aligned(16384))) uint32_t _frame_window[4096];

//Buffer for copying frames
__attribute__((aligned(16384))) uint32_t _frame_srcbuf[4096];
__attribute__((aligned(16384))) uint32_t _frame_dstbuf[4096];

size_t m_frame_size(void)
{
	//We put 4 small-pages together into a 16KByte frame.
	//This is necessary because the top-level translation table must be 16KByte-aligned.
	return 16384;
}

uintptr_t m_frame_alloc(void)
{
	if(_frame_window[0] != 0)
	{
		uintptr_t retval = m_kspc_get((uintptr_t)_frame_window);
		m_kspc_set((uintptr_t)_frame_window, _frame_window[0]);
		return retval;
	}
	
	return 0;
}

void m_frame_free(uintptr_t frame)
{
	uintptr_t old_head = m_kspc_get((uintptr_t)_frame_window);
	m_kspc_set((uintptr_t)_frame_window, frame);
	_frame_window[0] = old_head;
}

void m_frame_copy(uintptr_t newframe, uintptr_t oldframe)
{
	m_kspc_set((uintptr_t)_frame_dstbuf, newframe);
	m_kspc_set((uintptr_t)_frame_srcbuf, oldframe);
	memcpy(_frame_dstbuf, _frame_srcbuf, sizeof(_frame_dstbuf));
}

