//ps2kbd.h
//PS/2 keyboard state
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef PS2KBD_H
#define PS2KBD_H

#include <stdint.h>

//Initializes keyboard.
void ps2kbd_init(void);

//Called when new data is received on PS/2 port.
void ps2kbd_isr(uint8_t ps2data);

#endif //PS2KBD_H

