//pspace.h
//Physical space access on AMD64
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef PSPACE_H
#define PSPACE_H

#include <stdint.h>

//Reads from physical space
uint64_t pspace_read(uint64_t addr);

//Writes to physical space
void pspace_write(uint64_t addr, uint64_t data);

//Clears a frame of physical space
void pspace_clrframe(uint64_t addr);


#endif //PSPACE_H

