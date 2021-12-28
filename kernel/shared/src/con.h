//con.h
//Console handling in kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef CON_H
#define CON_H

#include <sys/types.h>
#include <sc_con.h>
#include <stdbool.h>

//Sets the thread which receives unpauses from the console.
void con_settid(id_t tid);

//Outputs pending keyboard/mouse input.
ssize_t con_input(void *buf, ssize_t buflen);

//Called on interrupt when keyboard input occurs.
void con_isr_kbd(_sc_con_scancode_t scancode, bool state);

#endif //CON_H

