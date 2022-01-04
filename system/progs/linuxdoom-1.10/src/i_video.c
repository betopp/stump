// Rewritten by Bryan E. Topp, 2021
//
// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <unistd.h>

#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <signal.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

#include <sc.h>

//
//  Translates the key currently in X_event
//

int xlatekey(int input)
{
    int rc = input;
    switch(rc)
    {
      case _SC_CON_SCANCODE_LEFT: rc = KEY_LEFTARROW; break;
      case _SC_CON_SCANCODE_RIGHT: rc = KEY_RIGHTARROW; break;
      case _SC_CON_SCANCODE_DOWN: rc = KEY_DOWNARROW; break;
      case _SC_CON_SCANCODE_UP: rc = KEY_UPARROW; break;
      case _SC_CON_SCANCODE_ESCAPE: rc = KEY_ESCAPE; break;
      case _SC_CON_SCANCODE_RETURN: rc = KEY_ENTER; break;
      case _SC_CON_SCANCODE_TAB: rc = KEY_TAB; break;
      case _SC_CON_SCANCODE_F1: rc = KEY_F1; break;
      case _SC_CON_SCANCODE_F2: rc = KEY_F2; break;
      case _SC_CON_SCANCODE_F3: rc = KEY_F3; break;
      case _SC_CON_SCANCODE_F4: rc = KEY_F4; break;
      case _SC_CON_SCANCODE_F5: rc = KEY_F5; break;
      case _SC_CON_SCANCODE_F6: rc = KEY_F6; break;
      case _SC_CON_SCANCODE_F7: rc = KEY_F7; break;
      case _SC_CON_SCANCODE_F8: rc = KEY_F8; break;
      case _SC_CON_SCANCODE_F9: rc = KEY_F9; break;
      case _SC_CON_SCANCODE_F10: rc = KEY_F10; break;
      case _SC_CON_SCANCODE_F11: rc = KEY_F11; break;
      case _SC_CON_SCANCODE_F12: rc = KEY_F12; break;
	
      case _SC_CON_SCANCODE_BACKSPACE:
      case _SC_CON_SCANCODE_DELETE: rc = KEY_BACKSPACE; break;

      case _SC_CON_SCANCODE_PAUSE: rc = KEY_PAUSE; break;

      case _SC_CON_SCANCODE_KP_EQUALS:
      case _SC_CON_SCANCODE_EQUALS: rc = KEY_EQUALS; break;

      case _SC_CON_SCANCODE_KP_MINUS:
      case _SC_CON_SCANCODE_MINUS: rc = KEY_MINUS; break;

      case _SC_CON_SCANCODE_LSHIFT:
      case _SC_CON_SCANCODE_RSHIFT:
	rc = KEY_RSHIFT;
	break;
	
      case _SC_CON_SCANCODE_LCTRL:
      case _SC_CON_SCANCODE_RCTRL:
	rc = KEY_RCTRL;
	break;
	
      case _SC_CON_SCANCODE_LALT:
      case _SC_CON_SCANCODE_LGUI:
      case _SC_CON_SCANCODE_RALT:
      case _SC_CON_SCANCODE_RGUI:
	rc = KEY_RALT;
	break;
	
      default:
	if (rc >= _SC_CON_SCANCODE_1 && rc <= _SC_CON_SCANCODE_9)
	    rc = rc - _SC_CON_SCANCODE_1 + '1';
	else if (rc >= _SC_CON_SCANCODE_SPACE && rc <= _SC_CON_SCANCODE_GRAVE)
	    rc = rc - _SC_CON_SCANCODE_SPACE + ' ';
	else if (rc >= _SC_CON_SCANCODE_A && rc <= _SC_CON_SCANCODE_Z)
	    rc = rc - _SC_CON_SCANCODE_A + 'a';
	break;
    }

    return rc;

}

void I_ShutdownGraphics(void) { }
void I_StartFrame (void) { }


boolean		mousemoved = false;

void HandleEvent(_sc_con_input_t *input)
{
	event_t event = {0};
	
	switch(input->type.type)
	{
		case _SC_CON_INPUT_TYPE_KBD:
		{
			event.type = input->kbd.state ? ev_keydown : ev_keyup;
			event.data1 = xlatekey(input->kbd.scancode);
			D_PostEvent(&event);
		}
		case _SC_CON_INPUT_TYPE_MOUSE:
		{
			break;
		}
		default:
		{
			break;
		}
	}
}



    // put event-grabbing stuff in here
/*    XNextEvent(X_display, &X_event);
    switch (X_event.type)
    {
      case KeyPress:
	event.type = ev_keydown;
	event.data1 = xlatekey();
	D_PostEvent(&event);
	// fprintf(stderr, "k");
	break;
      case KeyRelease:
	event.type = ev_keyup;
	event.data1 = xlatekey();
	D_PostEvent(&event);
	// fprintf(stderr, "ku");
	break;
      case ButtonPress:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xbutton.state & Button1Mask)
	    | (X_event.xbutton.state & Button2Mask ? 2 : 0)
	    | (X_event.xbutton.state & Button3Mask ? 4 : 0)
	    | (X_event.xbutton.button == Button1)
	    | (X_event.xbutton.button == Button2 ? 2 : 0)
	    | (X_event.xbutton.button == Button3 ? 4 : 0);
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	// fprintf(stderr, "b");
	break;
      case ButtonRelease:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xbutton.state & Button1Mask)
	    | (X_event.xbutton.state & Button2Mask ? 2 : 0)
	    | (X_event.xbutton.state & Button3Mask ? 4 : 0);
	// suggest parentheses around arithmetic in operand of |
	event.data1 =
	    event.data1
	    ^ (X_event.xbutton.button == Button1 ? 1 : 0)
	    ^ (X_event.xbutton.button == Button2 ? 2 : 0)
	    ^ (X_event.xbutton.button == Button3 ? 4 : 0);
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	// fprintf(stderr, "bu");
	break;
      case MotionNotify:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xmotion.state & Button1Mask)
	    | (X_event.xmotion.state & Button2Mask ? 2 : 0)
	    | (X_event.xmotion.state & Button3Mask ? 4 : 0);
	event.data2 = (X_event.xmotion.x - lastmousex) << 2;
	event.data3 = (lastmousey - X_event.xmotion.y) << 2;

	if (event.data2 || event.data3)
	{
	    lastmousex = X_event.xmotion.x;
	    lastmousey = X_event.xmotion.y;
	    if (X_event.xmotion.x != X_width/2 &&
		X_event.xmotion.y != X_height/2)
	    {
		D_PostEvent(&event);
		// fprintf(stderr, "m");
		mousemoved = false;
	    } else
	    {
		mousemoved = true;
	    }
	}
	break;
	
      case Expose:
      case ConfigureNotify:
	break;
	
      default:
	if (doShm && X_event.type == X_shmeventtype) shmFinished = true;
	break;
    }
*/


//
// I_StartTic
//
void I_StartTic (void)
{
	while(1)
	{
		_sc_con_input_t from_con_buf[16];
		ssize_t from_con_len = _sc_con_input(from_con_buf, sizeof(from_con_buf[0]), sizeof(from_con_buf));
		if(from_con_len <= 0)
			break;
		
		for(uint32_t ii = 0; ii < from_con_len / sizeof(_sc_con_input_t); ii++)
		{
			HandleEvent(&(from_con_buf[ii]));
		}
	}

	mousemoved = false;

}

void I_UpdateNoBlit (void) { }

uint32_t last_palette[256];
uint32_t truecolor[SCREENHEIGHT*SCREENWIDTH];

void I_FinishUpdate (void)
{
    for(int ii = 0; ii < SCREENWIDTH * SCREENHEIGHT; ii++)
    {
	    truecolor[ii] = last_palette[screens[0][ii]];
    }

    _sc_con_flip(truecolor, 0);
    //_sc_pause(); //Make sure we're pausing at least once per tick - doom defines a "waitVBL" but doesn't use it
}

void I_ReadScreen (byte* scr)
{
    memcpy (scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}

void I_SetPalette (byte* palette)
{
	for(int cc = 0; cc < 256; cc++)
	{
		last_palette[cc] = 0;
		last_palette[cc] |= (uint32_t)(*(palette++)) <<  16;
		last_palette[cc] |= (uint32_t)(*(palette++)) <<  8;
		last_palette[cc] |= (uint32_t)(*(palette++)) << 0;
		last_palette[cc] |= 0xFF000000;
	}
}


void I_InitGraphics(void)
{
	static int		firsttime=1;
	if (!firsttime)
		return;
	
	firsttime = 0;

	signal(SIGINT, (void (*)(int)) I_Quit);

	screens[0] = (unsigned char *) malloc (SCREENWIDTH * SCREENHEIGHT);

	const _sc_con_init_t con_init = 
	{
		.flags = 0,
		.fb_width = SCREENWIDTH,
		.fb_height = SCREENHEIGHT,
		.fb_stride = SCREENWIDTH * sizeof(uint32_t)
	};
	
	int con_init_err = _sc_con_init(&con_init, sizeof(con_init));
	if(con_init_err < 0)
	{
		I_Error("con init failed");
	}

}

int inited;
