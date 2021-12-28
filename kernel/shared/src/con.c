//con.c
//Console handling in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "con.h"
#include "process.h"
#include "thread.h"
#include <errno.h>

//Console input ring-buffer
#define CON_INBUF_MAX 1024
static volatile _sc_con_input_t con_inbuf[CON_INBUF_MAX];
static volatile m_atomic_t con_inbuf_r;
static volatile m_atomic_t con_inbuf_w;

//Which thread gets unpaused when console input occurs
static id_t con_tid;

void con_settid(id_t tid)
{
	if(tid != con_tid)
	{
		con_tid = tid;
		thread_unpause(con_tid); //Kick them now that they've become the console thread
	}
}

ssize_t con_input(void *buf, ssize_t buflen)
{
	_sc_con_input_t *bufobj = (_sc_con_input_t*)(buf);
	ssize_t retval = 0;
	while(buflen >= (ssize_t)sizeof(*bufobj))
	{
		m_atomic_t latest_w = con_inbuf_w;
		m_atomic_t latest_r = con_inbuf_r;		
		if(latest_w == latest_r)
		{
			//Ringbuffer is empty.
			break;
		}
		
		_sc_con_input_t latest_input = con_inbuf[latest_r];
		m_atomic_t next_r = (latest_r + 1) % CON_INBUF_MAX;
		bool advanced = m_atomic_cmpxchg(&con_inbuf_r, latest_r, next_r);
		if(!advanced)
		{
			//Somebody else is reading from underneath us (!?!?)
			//This can probably happen if one process has the console and tries to use it from multiple threads.
			return -EBUSY;
		}
		
		*bufobj = latest_input;
		buflen -= sizeof(*bufobj);
		retval += sizeof(*bufobj);
		bufobj++;
	}
	
	if(retval == 0)
		return -EAGAIN; //No input available
	
	return retval;
}

void con_isr_kbd(_sc_con_scancode_t scancode, bool state)
{
	m_atomic_t latest_r = con_inbuf_r;
	m_atomic_t latest_w = con_inbuf_w;
	m_atomic_t next_w = (latest_w + 1) % CON_INBUF_MAX;
	if(next_w == latest_r)
	{
		//Advancing the write-pointer would make the read- and write-pointers equal.
		//That's the empty condition - we'd have overflowed. Drop the input.
		return;
	}
	
	const _sc_con_input_t newinput =
	{
		.kbd = 
		{
			.scancode = scancode,
			.state = state,
			.unused = 0,
			.type = _SC_CON_INPUT_TYPE_KBD,
		},
	};
	
	con_inbuf[latest_w] = newinput;
	m_atomic_cmpxchg(&con_inbuf_w, latest_w, next_w);
	
	thread_unpause(con_tid);
}
