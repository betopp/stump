//sc.c
//System call entry points from user-mode
//Bryan E. Topp <betopp@betopp.com> 2021

#include <sc.h>
#include <sc_win.h>

//Type that is supported for passing parameters/returns in system calls
#define SYSTYPE uintptr_t

//Per-architecture system call support, by number of parameters
extern SYSTYPE _sc0(SYSTYPE num);
extern SYSTYPE _sc1(SYSTYPE num, SYSTYPE p1);
extern SYSTYPE _sc2(SYSTYPE num, SYSTYPE p1, SYSTYPE p2);
extern SYSTYPE _sc3(SYSTYPE num, SYSTYPE p1, SYSTYPE p2, SYSTYPE p3);
extern SYSTYPE _sc4(SYSTYPE num, SYSTYPE p1, SYSTYPE p2, SYSTYPE p3, SYSTYPE p4);
extern SYSTYPE _sc5(SYSTYPE num, SYSTYPE p1, SYSTYPE p2, SYSTYPE p3, SYSTYPE p4, SYSTYPE p5);

//Use macro-trick to define entry points for each user-facing system call.

#define SYSCALL0R(num, rtype, name) \
	rtype name(void) \
	{ \
		return (rtype)_sc0(num); \
	}

#define SYSCALL1R(num, rtype, name, p1t) \
	rtype name(p1t p1) \
	{ \
		return (rtype)_sc1(num, (SYSTYPE)p1); \
	}
	
#define SYSCALL2R(num, rtype, name, p1t, p2t) \
	rtype name(p1t p1, p2t p2) \
	{ \
		return (rtype)_sc2(num, (SYSTYPE)p1, (SYSTYPE)p2); \
	}
	
#define SYSCALL3R(num, rtype, name, p1t, p2t, p3t) \
	rtype name(p1t p1, p2t p2, p3t p3) \
	{ \
		return (rtype)_sc3(num, (SYSTYPE)p1, (SYSTYPE)p2, (SYSTYPE)p3); \
	}

#define SYSCALL4R(num, rtype, name, p1t, p2t, p3t, p4t) \
	rtype name(p1t p1, p2t p2, p3t p3, p4t p4) \
	{ \
		return (rtype)_sc4(num, (SYSTYPE)p1, (SYSTYPE)p2, (SYSTYPE)p3, (SYSTYPE)p4); \
	}
	
#define SYSCALL5R(num, rtype, name, p1t, p2t, p3t, p4t, p5t) \
	rtype name(p1t p1, p2t p2, p3t p3, p4t p4, p5t p5) \
	{ \
		return (rtype)_sc5(num, (SYSTYPE)p1, (SYSTYPE)p2, (SYSTYPE)p3, (SYSTYPE)p4, (SYSTYPE)p5); \
	}

#define SYSCALL0V(num, rtype, name) \
	rtype name(void) \
	{ \
		_sc0(num); \
	}

#define SYSCALL1V(num, rtype, name, p1t) \
	rtype name(p1t p1) \
	{ \
		_sc1(num, (SYSTYPE)p1); \
	}
	
#define SYSCALL2V(num, rtype, name, p1t, p2t) \
	rtype name(p1t p1, p2t p2) \
	{ \
		_sc2(num, (SYSTYPE)p1, (SYSTYPE)p2); \
	}
	
#define SYSCALL3V(num, rtype, name, p1t, p2t, p3t) \
	rtype name(p1t p1, p2t p2, p3t p3) \
	{ \
		_sc3(num, (SYSTYPE)p1, (SYSTYPE)p2, (SYSTYPE)p3); \
	}

#define SYSCALL4V(num, rtype, name, p1t, p2t, p3t, p4t) \
	rtype name(p1t p1, p2t p2, p3t p3, p4t p4) \
	{ \
		_sc4(num, (SYSTYPE)p1, (SYSTYPE)p2, (SYSTYPE)p3, (SYSTYPE)p4); \
	}
	
#define SYSCALL5V(num, rtype, name, p1t, p2t, p3t, p4t, p5t) \
	rtype name(p1t p1, p2t p2, p3t p3, p4t p4, p5t p5) \
	{ \
		_sc5(num, (SYSTYPE)p1, (SYSTYPE)p2, (SYSTYPE)p3, (SYSTYPE)p4, (SYSTYPE)p5); \
	}
	
#define SYSCALL0N(num, rtype, name) \
	rtype name(void) \
	{ \
		_sc0(num); \
		while(1) { } \
	}

#define SYSCALL1N(num, rtype, name, p1t) \
	rtype name(p1t p1) \
	{ \
		_sc1(num, (SYSTYPE)p1); \
		while(1) { } \
	}
	
#define SYSCALL2N(num, rtype, name, p1t, p2t) \
	rtype name(p1t p1, p2t p2) \
	{ \
		_sc2(num, (SYSTYPE)p1, (SYSTYPE)p2); \
		while(1) { } \
	}
	
#define SYSCALL3N(num, rtype, name, p1t, p2t, p3t) \
	rtype name(p1t p1, p2t p2, p3t p3) \
	{ \
		_sc3(num, (SYSTYPE)p1, (SYSTYPE)p2, (SYSTYPE)p3); \
		while(1) { } \
	}

#define SYSCALL4N(num, rtype, name, p1t, p2t, p3t, p4t) \
	rtype name(p1t p1, p2t p2, p3t p3, p4t p4) \
	{ \
		_sc4(num, (SYSTYPE)p1, (SYSTYPE)p2, (SYSTYPE)p3, (SYSTYPE)p4); \
		while(1) { } \
	}
	
#define SYSCALL5N(num, rtype, name, p1t, p2t, p3t, p4t, p5t) \
	rtype name(p1t p1, p2t p2, p3t p3, p4t p4, p5t p5) \
	{ \
		_sc5(num, (SYSTYPE)p1, (SYSTYPE)p2, (SYSTYPE)p3, (SYSTYPE)p4, (SYSTYPE)p5); \
		while(1) { } \
	}
	
#include "systable.h"
