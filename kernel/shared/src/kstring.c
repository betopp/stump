//kstring.c
//String manipulation functions for kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include <string.h>
#include <stdint.h>
#include <stddef.h>

char *strncpy(char *dst, const char *src, size_t n)
{
	size_t ii = 0;
	while(ii < n)
	{
		if(src[ii] == '\0')
			break;
		
		dst[ii] = src[ii];
		ii++;
	}
	
	while(ii < n)
	{
		dst[ii] = 0;
		ii++;
	}
	
	return dst;
}

size_t strlen(const char *s)
{
	size_t l = 0;
	while(s[l] != '\0')
	{
		l++;
	}
	return l;
}

int strcmp(const char *s1, const char *s2)
{
	while(1)
	{
		if(*s1 != *s2)
			return *s1 - *s2;
		
		if(*s1 == '\0')
			return 0;
		
		s1++;
		s2++;
	}
}

char *strchr(const char *s, int c)
{
	while(1)
	{
		if(*s == c)
			return (char*)s; //Note - checked before end-of-string, so c can be \0
		
		if(*s == '\0')
			return NULL;
		
		s++;
	}
}

int memcmp(const void *s1v, const void *s2v, size_t len)
{
	const char *s1 = (const char*)s1v;
	const char *s2 = (const char*)s2v;
	
	for(size_t ll = 0; ll < len; ll++)
	{
		if(s1[ll] != s2[ll])
			return s1[ll] - s2[ll];
	}
	
	return 0;
}

void *memset(void *dst, int c, size_t len)
{
	uint8_t *dst_b = (uint8_t*)(dst);
	for(size_t ll = 0; ll < len; ll++)
	{
		dst_b[ll] = c;
	}
	return dst;
}

void *memcpy(void *dst, const void *src, size_t len)
{
	uint8_t *dstb = (uint8_t*)dst;
	const uint8_t *srcb = (const uint8_t*)src;
	for(size_t ll = 0; ll < len; ll++)
	{
		dstb[ll] = srcb[ll];
	}
	return dst;
}

void *memmove(void *dst, const void *src, size_t len)
{
	const char *sb = (const char*)src;
	char *db = (char*)dst;
	if(db > sb)
	{
		for(size_t ii = len; ii > 0; ii--)
		{
			db[ii-1] = sb[ii-1];
		}
	}
	else
	{
		for(size_t ii = 0; ii < len; ii++)
		{
			db[ii] = sb[ii];
		}
	}
	return dst;
}
