//syscalls.h
//System call declarations on kernel-side
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <sc.h>

//Handles a system-call based on call number.
uintptr_t syscalls_handle(uintptr_t num, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, uintptr_t p5);

//Use macro-trick and system call table to declare all system call handlers.

#define SYSCALL0R(num, rt,  name)                          rt k##name(void);
#define SYSCALL1R(num, rt,  name, p1t)                     rt k##name(p1t p1);
#define SYSCALL2R(num, rt,  name, p1t, p2t)                rt k##name(p1t p1, p2t p2);
#define SYSCALL3R(num, rt,  name, p1t, p2t, p3t)           rt k##name(p1t p1, p2t p2, p3t p3);
#define SYSCALL4R(num, rt,  name, p1t, p2t, p3t, p4t)      rt k##name(p1t p1, p2t p2, p3t p3, p4t p4);
#define SYSCALL5R(num, rt,  name, p1t, p2t, p3t, p4t, p5t) rt k##name(p1t p1, p2t p2, p3t p3, p4t p4, p5t p5);

#define SYSCALL0V(num, rt,  name)                          rt k##name(void);
#define SYSCALL1V(num, rt,  name, p1t)                     rt k##name(p1t p1);
#define SYSCALL2V(num, rt,  name, p1t, p2t)                rt k##name(p1t p1, p2t p2);
#define SYSCALL3V(num, rt,  name, p1t, p2t, p3t)           rt k##name(p1t p1, p2t p2, p3t p3);
#define SYSCALL4V(num, rt,  name, p1t, p2t, p3t, p4t)      rt k##name(p1t p1, p2t p2, p3t p3, p4t p4);
#define SYSCALL5V(num, rt,  name, p1t, p2t, p3t, p4t, p5t) rt k##name(p1t p1, p2t p2, p3t p3, p4t p4, p5t p5);

#define SYSCALL0N(num, rt,  name)                          rt k##name(void);
#define SYSCALL1N(num, rt,  name, p1t)                     rt k##name(p1t p1);
#define SYSCALL2N(num, rt,  name, p1t, p2t)                rt k##name(p1t p1, p2t p2);
#define SYSCALL3N(num, rt,  name, p1t, p2t, p3t)           rt k##name(p1t p1, p2t p2, p3t p3);
#define SYSCALL4N(num, rt,  name, p1t, p2t, p3t, p4t)      rt k##name(p1t p1, p2t p2, p3t p3, p4t p4);
#define SYSCALL5N(num, rt,  name, p1t, p2t, p3t, p4t, p5t) rt k##name(p1t p1, p2t p2, p3t p3, p4t p4, p5t p5);

#include "systable.h"

#undef SYSCALL0N
#undef SYSCALL1N
#undef SYSCALL2N
#undef SYSCALL3N
#undef SYSCALL4N
#undef SYSCALL5N
#undef SYSCALL0R
#undef SYSCALL1R
#undef SYSCALL2R
#undef SYSCALL3R
#undef SYSCALL4R
#undef SYSCALL5R
#undef SYSCALL0V
#undef SYSCALL1V
#undef SYSCALL2V
#undef SYSCALL3V
#undef SYSCALL4V
#undef SYSCALL5V

#endif //SYSCALLS_H
