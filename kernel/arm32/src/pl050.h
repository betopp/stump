//pl050.h
//ARM PS/2 controller support
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef PL050_H
#define PL050_H

//Initializes PL050
void pl050_init(void);

//Interrupt service routine for PL050
void pl050_isr(int which);

//Attempts to send a byte on PL050.
int pl050_send(int which, int ch);

//Polls for reception of a byte on PL050.
int pl050_recv(int which);

#endif //PL050_H
