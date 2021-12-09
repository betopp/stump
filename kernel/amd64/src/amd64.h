//amd64.h
//Common definitions for AMD64 machine support
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef AMD64_H
#define AMD64_H

#include <stdint.h>

//Sets the Page Directory Base Register
static inline void setcr3(uint64_t val) { asm volatile ("mov %%rax, %%cr3": :"a" (val)); }

//Returns the Page Directory Base Register
static inline uint64_t getcr3(void) { uint64_t val; asm volatile ("mov %%cr3, %%rax": "=a"(val) : ); return val; }

//Invalidates the given page by virtual address
static inline void invlpg(uint64_t vaddr) { asm volatile ("invlpg (%%rax)": : "a" (vaddr)); }

//In/out instruction wrappers
static inline void outb(uint16_t port, uint8_t byte)
{
	asm volatile ("outb %%al,%%dx": :"d" (port), "a" (byte));
}

static inline void outw(uint16_t port, uint16_t word)
{
	asm volatile ("outw %%ax,%%dx": :"d" (port), "a" (word));
}

static inline void outd(uint16_t port, uint32_t dword)
{
	asm volatile ("outl %%eax,%%dx": :"d" (port), "a" (dword));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t byte;
	asm volatile ("inb %%dx,%%al":"=a"(byte):"d"(port));
	return byte;
}

static inline uint16_t inw(uint16_t port)
{
	uint16_t word;
	asm volatile ("inw %%dx,%%ax":"=a"(word):"d"(port));
	return word;
}

static inline uint32_t ind(uint16_t port)
{
	uint32_t dword;
	asm volatile ("inl %%dx,%%eax":"=a"(dword):"d"(port));
	return dword;
}

#endif //AMD64_H
