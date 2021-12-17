//argenv.h
//Argument/environment loading for new processes
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef ARGENV_H
#define ARGENV_H

#include "mem.h"

//Loads arg/env data into the memory space.
int argenv_load(mem_t *mem, char * const * argv, char * const * envp, uintptr_t *loaded_out);

#endif //ARGENV_H
