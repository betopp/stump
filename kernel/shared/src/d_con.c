//d_con.c
//Character device: computer console
//Bryan E. Topp <betopp@betopp.com> 2021

#include "d_con.h"
#include "kassert.h"
#include "process.h"
#include "sc_con.h"
#include "m_fb.h"
#include <errno.h>

//State of a console device
typedef struct d_con_s
{
	//How many files are open representing this console.
	int open;
	
} d_con_t;

//All console devices
#define D_CON_MAX 8
d_con_t d_con_table[D_CON_MAX];

int d_con_open(int minor)
{
	if(minor < 0 || minor >= D_CON_MAX)
		return -ENXIO;
	
	if(d_con_table[minor].open)
		return -EBUSY;
	
	d_con_table[minor].open++;
	KASSERT(d_con_table[minor].open == 1);
	return 0;
}

void d_con_close(int minor)
{
	KASSERT(minor >= 0 && minor < D_CON_MAX);
	KASSERT(d_con_table[minor].open == 1);
	d_con_table[minor].open--;
	return;
}

int d_con_ioctl(int minor, int operation, void *buf, ssize_t len)
{
	KASSERT(minor >= 0 && minor < D_CON_MAX);
	KASSERT(d_con_table[minor].open == 1);
	
	switch(operation)
	{
		case _SC_CON_IOCTL_FBGEOM:
		{
			_sc_con_fbgeom_t geom;
			uintptr_t paddr;
			m_fb_info(&paddr, &(geom.width), &(geom.height), &(geom.stride));
			if(len > (ssize_t)sizeof(geom))
				len = (ssize_t)sizeof(geom);
			int err = process_memput(buf, &geom, len);
			if(err < 0)
				return err;
			
			return len;
		}
		case _SC_CON_IOCTL_PAINT:
		{
			//Todo
			return -ENOSYS;
		}
		default:
			return -ENOTTY;
	}
}