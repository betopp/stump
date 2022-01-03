//mmlibc/include/mmbits/limits_llong.h
//Fragment for building C standard headers.
//Bryan E. Topp <betopp@betopp.com> 2020
#ifndef _LIMITS_LLONG_H
#define _LIMITS_LLONG_H

#define LLONG_MAX (__LONG_LONG_MAX__)
#define LLONG_MIN (-__LONG_LONG_MAX__-1)
#define ULLONG_MAX (18446744073709551615ULL) //GCC doesn't give us this?

#endif //_LIMITS_LLONG_H
