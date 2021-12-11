//elf64.h
//Handling of ELF format in kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef ELF64_H
#define ELF64_H

#include "file.h"
#include "mem.h"
#include <stdint.h>

//Loads the given ELF file into the given memory space.
int elf64_load(file_t *file, mem_t *mem, uintptr_t *entry_out);

#endif //ELF64_H
