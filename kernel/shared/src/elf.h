//elf.h
//Handling of ELF format in kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef ELF_H
#define ELF_H

#include "file.h"
#include "mem.h"
#include <stdint.h>

//Loads the given ELF file into the given memory space.
int elf_load(file_t *file, mem_t *mem, uintptr_t *entry_out);

#endif //ELF_H
