//mmlibc/include/mmbits/limits_long.h
//Fragment for building C standard headers.
//Bryan E. Topp <betopp@betopp.com> 2020
#ifndef _LIMITS_LONG_H
#define _LIMITS_LONG_H

#define LONG_MAX (__LONG_MAX__)
#define LONG_MIN (-__LONG_MAX__-1)
#define ULONG_MAX (18446744073709551615UL) //GCC doesn't give us this?

#endif //_LIMITS_LONG_H
