//mmlibc/include/ctype.h
//Character type declarations for MMK's libc.
//Bryan E. Topp <betopp@betopp.com> 2020
#ifndef _CTYPE_H
#define _CTYPE_H

#include <mmbits/typedef_locale.h>

int isalnum(int c);
int isalpha(int c);
int isascii(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);

int isalnum_l(int c, locale_t locale);
int isalpha_l(int c, locale_t locale);
int isblank_l(int c, locale_t locale);
int iscntrl_l(int c, locale_t locale);
int isdigit_l(int c, locale_t locale);
int isgraph_l(int c, locale_t locale);
int islower_l(int c, locale_t locale);
int isprint_l(int c, locale_t locale);
int ispunct_l(int c, locale_t locale);
int isspace_l(int c, locale_t locale);
int isupper_l(int c, locale_t locale);
int isxdigit_l(int c, locale_t locale);

int toascii(int c);
int tolower(int c);
int toupper(int c);

int tolower_l(int c, locale_t locale);
int toupper_l(int c, locale_t locale);

#endif // _CTYPE_H
