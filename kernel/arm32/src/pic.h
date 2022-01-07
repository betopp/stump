//pic.h
//Interrupt controller on QEMU ARM board
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef PIC_H
#define PIC_H

//Sets up interrupt controller
void pic_init(void);

//Handles interrupt
void pic_irq(void);

//Handles fast interrupt
void pic_fiq(void);

#endif //PIC_H
