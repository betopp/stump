//ps2kbd.c
//PS/2 keyboard implementation on i386
//Bryan E. Topp <betopp@betopp.com> 2021

#include "amd64.h"
#include <stdbool.h>
#include <sc.h>

//State of PS/2 keyboard - if a break prefix was received.
bool ps2kbd_break;

//State of PS/2 keyboard - if an extend prefix was received.
bool ps2kbd_extend;

//Mapping of PS/2 scancodes to SDL scancodes.
//Table for decoding PS/2 scan code set 2 to HAL scancodes.
//First 256 entries correspond to 1-byte make-codes, second 256 entries correspond to those following a 0xE0 code.
static const _sc_con_scancode_t ps2kbd_table[512] = 
{
	[0x00E] = _SC_CON_SCANCODE_GRAVE,
	[0x016] = _SC_CON_SCANCODE_1,
	[0x01E] = _SC_CON_SCANCODE_2,
	[0x026] = _SC_CON_SCANCODE_3,
	[0x025] = _SC_CON_SCANCODE_4,
	[0x02E] = _SC_CON_SCANCODE_5,
	[0x036] = _SC_CON_SCANCODE_6,
	[0x03D] = _SC_CON_SCANCODE_7,
	[0x03E] = _SC_CON_SCANCODE_8,
	[0x046] = _SC_CON_SCANCODE_9,
	[0x045] = _SC_CON_SCANCODE_0,
	[0x04E] = _SC_CON_SCANCODE_MINUS,
	[0x055] = _SC_CON_SCANCODE_EQUALS,
	[0x05D] = _SC_CON_SCANCODE_BACKSLASH,
	[0x066] = _SC_CON_SCANCODE_BACKSPACE,
	[0x00D] = _SC_CON_SCANCODE_TAB,
	[0x015] = _SC_CON_SCANCODE_Q,
	[0x01D] = _SC_CON_SCANCODE_W,
	[0x024] = _SC_CON_SCANCODE_E,
	[0x02D] = _SC_CON_SCANCODE_R,
	[0x02C] = _SC_CON_SCANCODE_T,
	[0x035] = _SC_CON_SCANCODE_Y,
	[0x03C] = _SC_CON_SCANCODE_U,
	[0x043] = _SC_CON_SCANCODE_I,
	[0x044] = _SC_CON_SCANCODE_O,
	[0x04D] = _SC_CON_SCANCODE_P,
	[0x054] = _SC_CON_SCANCODE_LEFTBRACKET,
	[0x05B] = _SC_CON_SCANCODE_RIGHTBRACKET,
	[0x058] = _SC_CON_SCANCODE_CAPSLOCK,
	[0x01C] = _SC_CON_SCANCODE_A,
	[0x01B] = _SC_CON_SCANCODE_S,
	[0x023] = _SC_CON_SCANCODE_D,
	[0x02B] = _SC_CON_SCANCODE_F,
	[0x034] = _SC_CON_SCANCODE_G,
	[0x033] = _SC_CON_SCANCODE_H,
	[0x03B] = _SC_CON_SCANCODE_J,
	[0x042] = _SC_CON_SCANCODE_K,
	[0x04B] = _SC_CON_SCANCODE_L,
	[0x04C] = _SC_CON_SCANCODE_SEMICOLON,
	[0x052] = _SC_CON_SCANCODE_APOSTROPHE,
	[0x05A] = _SC_CON_SCANCODE_RETURN,
	[0x012] = _SC_CON_SCANCODE_LSHIFT,
	[0x01A] = _SC_CON_SCANCODE_Z,
	[0x022] = _SC_CON_SCANCODE_X,
	[0x021] = _SC_CON_SCANCODE_C,
	[0x02A] = _SC_CON_SCANCODE_V,
	[0x032] = _SC_CON_SCANCODE_B,
	[0x031] = _SC_CON_SCANCODE_N,
	[0x03A] = _SC_CON_SCANCODE_M,
	[0x041] = _SC_CON_SCANCODE_COMMA,
	[0x049] = _SC_CON_SCANCODE_PERIOD,
	[0x04A] = _SC_CON_SCANCODE_SLASH,
	[0x059] = _SC_CON_SCANCODE_RSHIFT,
	[0x014] = _SC_CON_SCANCODE_LCTRL,
	[0x011] = _SC_CON_SCANCODE_LALT,
	[0x029] = _SC_CON_SCANCODE_SPACE,
	[0x111] = _SC_CON_SCANCODE_RALT,
	[0x114] = _SC_CON_SCANCODE_RCTRL,
	[0x170] = _SC_CON_SCANCODE_INSERT,
	[0x171] = _SC_CON_SCANCODE_DELETE,
	[0x16B] = _SC_CON_SCANCODE_LEFT,
	[0x16C] = _SC_CON_SCANCODE_HOME,
	[0x169] = _SC_CON_SCANCODE_END,
	[0x175] = _SC_CON_SCANCODE_UP,
	[0x172] = _SC_CON_SCANCODE_DOWN,
	[0x17D] = _SC_CON_SCANCODE_PAGEUP,
	[0x17A] = _SC_CON_SCANCODE_PAGEDOWN,
	[0x174] = _SC_CON_SCANCODE_RIGHT,
	[0x077] = _SC_CON_SCANCODE_NUMLOCKCLEAR,
	[0x06C] = _SC_CON_SCANCODE_KP_7,
	[0x06B] = _SC_CON_SCANCODE_KP_4,
	[0x069] = _SC_CON_SCANCODE_KP_1,
	[0x14A] = _SC_CON_SCANCODE_KP_DIVIDE,
	[0x075] = _SC_CON_SCANCODE_KP_8,
	[0x073] = _SC_CON_SCANCODE_KP_5,
	[0x072] = _SC_CON_SCANCODE_KP_2,
	[0x070] = _SC_CON_SCANCODE_KP_0,
	[0x07C] = _SC_CON_SCANCODE_KP_MULTIPLY,
	[0x07D] = _SC_CON_SCANCODE_KP_9,
	[0x074] = _SC_CON_SCANCODE_KP_6,
	[0x07A] = _SC_CON_SCANCODE_KP_3,
	[0x071] = _SC_CON_SCANCODE_KP_DECIMAL,
	[0x07B] = _SC_CON_SCANCODE_KP_MINUS,
	[0x079] = _SC_CON_SCANCODE_KP_PLUS,
	[0x15A] = _SC_CON_SCANCODE_KP_ENTER,
	[0x076] = _SC_CON_SCANCODE_ESCAPE,
	[0x005] = _SC_CON_SCANCODE_F1,
	[0x006] = _SC_CON_SCANCODE_F2,
	[0x004] = _SC_CON_SCANCODE_F3,
	[0x00C] = _SC_CON_SCANCODE_F4,
	[0x003] = _SC_CON_SCANCODE_F5,
	[0x00B] = _SC_CON_SCANCODE_F6,
	[0x083] = _SC_CON_SCANCODE_F7,
	[0x00A] = _SC_CON_SCANCODE_F8,
	[0x001] = _SC_CON_SCANCODE_F9,
	[0x009] = _SC_CON_SCANCODE_F10,
	[0x078] = _SC_CON_SCANCODE_F11,
	[0x007] = _SC_CON_SCANCODE_F12,
};


//Sends a 1-byte command to the PS/2 controller
static void ps2kbd_cmd1(uint8_t cmd)
{
	outb(0x64, cmd);
}

//Sends a 2-byte command to the PS/2 controller
static void ps2kbd_cmd2(uint8_t cmd, uint8_t parm)
{
	outb(0x64, cmd); //Send command
	while(inb(0x64) & 0x02) { } //Wait for controller's input buffer to be empty
	outb(0x60, parm); //Send data
}

//Sends data to the PS/2 port
static int ps2kbd_send(int dev, const void *buf, int size)
{
	if(size <= 0)
		return 0;
	
	//Check status register bit 1 - if it's set, we don't have room to send another byte now.
	if(inb(0x64) & 0x02)
		return 0;
	
	//Otherwise, enqueue one byte.
	//If we're talking to the second port, preface it with command 0xD4.
	if(dev)
	{
		outb(0x64, 0xD4);
		outb(0x60, *((uint8_t*)buf));
		return 1;
	}
	else
	{
		outb(0x60, *((uint8_t*)buf));
		return 1;
	}
}

void ps2kbd_init(void)
{	
	//Initialize PS/2 controller
	ps2kbd_cmd1(0xAD); //Disable first port
	ps2kbd_cmd1(0xA7); //Disable second port
	inb(0x60); //Read data to make sure there's no stray input
	ps2kbd_cmd2(0x60, 0x00); //Disable interrupts, enable clocks, disable translation
	ps2kbd_cmd1(0xAE); //Enable first port
	ps2kbd_cmd1(0xA8); //Enable second port
	ps2kbd_cmd2(0x60, 0x03); //Enable interrupts and disable scancode translation
	inb(0x60); //Read data to make sure there's no stray input
	
	//Reset PS/2 devices
	const uint8_t rstcmd = 0xFF;
	while(!ps2kbd_send(0, &rstcmd, 1)) { }
	while(!ps2kbd_send(1, &rstcmd, 1)) { }
	
	//Turn on mouse
	const uint8_t stream = 0xEA;
	while(!ps2kbd_send(1, &stream, 1)) { }
}

void ps2kbd_isr(void)
{
	//Get the byte of data from the PS/2 controller
	uint8_t ps2data = inb(0x60);
	
	//If the data byte is a break-code prefix, or an extended keycode prefix, process that.
	if(ps2data == 0xF0)
	{
		//This is a break-code prefix.
		//The following bytes will specify which key is breaking, rather than making.
		ps2kbd_break = true;
		return;
	}
	
	if(ps2data == 0xE0)
	{
		//This is an extended keycode prefix.
		//The following byte will be looked up in the extended scancode table.
		ps2kbd_extend = true;
		return;
	}
	
	//Otherwise, decode the scancode and see if it's a key we care about
	int table_idx = ps2data + (ps2kbd_extend ? 256 : 0);
	if(ps2kbd_table[table_idx] != 0)
	{
		extern void entry_kbd();
		entry_kbd(ps2kbd_table[table_idx], !ps2kbd_break);
	}

	//Clear prefix flags, now that we've read their following byte.
	ps2kbd_break = false;
	ps2kbd_extend = false;
}



