//scanf.c
//Definitions for scanf family for libc
//Bryan E. Topp <betopp@betopp.com> 2021

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>
#include <limits.h>

bool _scanf_trymatch_num(FILE *stream, int base, long long int *val_out)
{
	unsigned long long val = 0;
	int digits = 0;
	bool neg = false;
	
	int signch = fgetc(stream);
	if(signch == '+')
	{
		neg = false;
	}
	else if(signch == '-')
	{
		neg = true;
	}
	else
	{
		neg = false;
		ungetc(signch, stream);
	}
	
	if(base == 0)
	{
		int basech = fgetc(stream);
		if(basech == '0')
		{
			int basech2 = fgetc(stream);
			if(basech2 == 'X' || basech2 == 'x')
			{
				base = 16;
			}
			else
			{
				base = 8;
				ungetc(basech2, stream);
			}
		}
		else
		{
			base = 10;
			ungetc(basech, stream);
		}
	}
	
	while(1)
	{
		int ch = fgetc(stream);
		if(ch < '0' || ch >= ('0' + base))
		{
			ungetc(ch, stream);
			*val_out = neg ? (-val) : val;
			return (digits > 0);
		}
		
		val *= base;
		val += ch;
		digits++;
	}
}

bool _scanf_trymatch_whitespace(FILE *stream)
{
	int ch = fgetc(stream);
	if(isspace(ch))
		return true;
	
	ungetc(ch, stream);
	return false;
}

bool _scanf_trymatch_literal(FILE *stream, char literal)
{
	int ch = fgetc(stream);
	if(ch == literal)
		return true;
	
	ungetc(ch, stream);
	return false;
}

//Returns the right return value for scanf functions, depending on tokens matched and error status.
int _scanf_retval(FILE *stream, int nmatched)
{
	if(nmatched > 0)
		return nmatched;
	
	if(stream->eof || stream->error)
		return EOF;
	
	return 0;
}

//We implement the bulk of this family in vfscanf.
//Other scanf-family functions eventually call vfscanf.
//vfscanf uses fgetc, which handles all sources we scan from.
int vfscanf(FILE *stream, const char *format, va_list ap)
{
	//Return value should be the number of tokens scanned.
	int nmatched = 0;
	
	//Work through format string until we reach its NUL terminator.
	while(*format != '\0')
	{	
		//Whitespace in format matches any amount of whitespace from input, including none.
		if(isspace(*format))
		{
			bool matched = _scanf_trymatch_whitespace(stream);
			if(matched)
			{
				//Continue trying to match more input whitespace for this format whitespace.
				continue;
			}
			else
			{
				//Done matching whitespace for this format char.
				format++;
				continue;
			}
		}
		
		//Any other format string, except a conversion specifier, must be matched exactly.
		if(*format != '%')
		{
			bool matched = _scanf_trymatch_literal(stream, *format);
			if(matched)
			{
				//Matched the literal. Keep going.
				format++;
				continue;
			}
			else
			{
				//Didn't match
				return _scanf_retval(stream, nmatched);
			}
		}
		
		//Looking at a percent symbol, starting a conversion specification. Advance past the percent.
		format++;
		
		//See if we're discarding the input once we've parsed it.
		bool discard = false;
		if(*format == '*')
		{
			discard = true;
			format++;
		}
		
		//See if we match a length
		int maxlen = 0;
		while(isdigit(*format))
		{
			maxlen *= 10;
			maxlen += *format - '0';
			format++;
		}
		
		//Consume optional length modifier.
		char length_mod[2] = {0};
		bool no_more_length = false;
		while(!no_more_length)
		{
			switch(*format)
			{				
				case 'h':
					if(length_mod[0] == 'h')
						length_mod[1] = 'h'; //Second occurrance
					
					length_mod[0] = 'h';
					format++;
					break;
				case 'l':
					if(length_mod[0] == 'l')
						length_mod[1] = 'l'; //Second occurrance
					
					length_mod[0] = 'l';
					format++;
					break;
				case 'L':
					length_mod[0] = 'L';
					format++;
					break;
				case 'j':
					length_mod[0] = 'j';
					format++;
					break;
				case 'z':
					length_mod[0] = 'z';
					format++;
					break;
				case 't':
					length_mod[0] = 't';
					format++;
					break;
				default:
					no_more_length = true;
					break;
			}
		}
		
		//Consume conversion specifier and try to perform matching.
		long long int num_val = 0;
		bool num_matched = false;
		bool num_signed = false;
		bool string_matched = false;
		switch(*format)
		{
			case 'd':
				num_matched = _scanf_trymatch_num(stream, 10, &num_val);
				num_signed = true;
				break;
			case 'i':
				num_matched = _scanf_trymatch_num(stream, 0, &num_val);
				num_signed = true;
				break;
	
			case 'o':
				num_matched = _scanf_trymatch_num(stream, 8, &num_val);
				num_signed = false;
				break;
			
			case 'u':
				num_matched = _scanf_trymatch_num(stream, 10, &num_val);
				num_signed = false;
				break;
			
			case 'x':
			case 'X':
				num_matched = _scanf_trymatch_num(stream, 16, &num_val);
				num_signed = false;
				break;
			
			case 'e':
			case 'E':
			case 'f':
			case 'F':
			case 'g':
			case 'G':
			case 'a':
			case 'A':
				break;
			
			//Character, or wide-character as multibyte
			case 'c':
				break;
			
			//String
			case 's':
			{
				char *dst = NULL;
				if(!discard)
					dst = va_arg(ap, char*);
				
				if(maxlen == 0)
					maxlen = INT_MAX - 1;
				
				while(maxlen > 0)
				{
					int nextchar = fgetc(stream);
					if(nextchar == EOF || isspace(nextchar))
						break;
				
					*dst = nextchar;
					dst++;
					string_matched = true;
					maxlen--;
				}
					
				break;
			}
			default:
				break;
			
		}
		format++;

		//Store the converted result, if desired
		if(num_matched && !discard)
		{
			if(num_signed)
			{
				if(length_mod[0] == 'h' && length_mod[1] == 'h')
				{
					//Converting %hhd, signed char.
					*(va_arg(ap, signed char*)) = num_val;
				}
				else if(length_mod[0] == 'h')
				{
					//Converting %hd, short int.
					*(va_arg(ap, short int*)) = num_val;
				}
				else if(length_mod[0] == 'l' && length_mod[1] == 'l')
				{
					//Converting %lld, long long int.
					*(va_arg(ap, long long int*)) = num_val;
				}
				else if(length_mod[0] == 'l')
				{
					//Converting %ld, long int.
					*(va_arg(ap, long int*)) = num_val;
				}
				else if(length_mod[0] == 'j')
				{
					//Converting %jd, intmax_t.
					*(va_arg(ap, intmax_t*)) = num_val;
				}
				else if(length_mod[0] == 'z')
				{
					//Converting %zd, ssize_t.
					*(va_arg(ap, ssize_t*)) = num_val;
				}
				else if(length_mod[0] == 't')
				{
					//Converting %td, ptrdiff_t.
					*(va_arg(ap, ptrdiff_t*)) = num_val;
				}
				else
				{						
					//Converting %d, int.
					*(va_arg(ap, int*)) = num_val;
				}
			}
			else
			{
				if(length_mod[0] == 'h' && length_mod[1] == 'h')
				{
					//Converting %hhu, unsigned char.
					*(va_arg(ap, unsigned int*)) = num_val;
				}
				else if(length_mod[0] == 'h')
				{
					//Converting %hu, unsigned short.
					*(va_arg(ap, unsigned int*)) = num_val;
				}
				else if(length_mod[0] == 'l' && length_mod[1] == 'l')
				{
					//Converting %llu, unsigned long long.
					*(va_arg(ap, unsigned long long*)) = num_val;
				}
				else if(length_mod[0] == 'l')
				{
					//Converting %lu, unsigned long.
					*(va_arg(ap, unsigned long*)) = num_val;
				}
				else if(length_mod[0] == 'j')
				{
					//Converting %ju, uintmax_t.
					*(va_arg(ap, intmax_t*)) = num_val;
				}
				else if(length_mod[0] == 'z')
				{
					//Converting %zu, size_t.
					*(va_arg(ap, ssize_t*)) = num_val;
				}
				else if(length_mod[0] == 't')
				{
					//Converting %tu, ptrdiff_t... as unsigned? Is this meaningful?
					*(va_arg(ap, ptrdiff_t*)) = num_val;
				}
				else
				{						
					//Converting %u, unsigned int.
					*(va_arg(ap, unsigned int*)) = num_val;
				}
			}
		}
		
		//Count how many conversions were performed.
		if(num_matched || string_matched)
			nmatched++;
	}
	
	return nmatched;
}


int scanf(const char *format, ...)
{
	//Set up va_list and call "v" version.
	va_list args;
	va_start(args, format);
	int retval = vscanf(format, args);
	va_end(args);
	return retval;
}

int fscanf(FILE *stream, const char *format, ...)
{
	//Set up va_list and call "v" version.
	va_list args;
	va_start(args, format);
	int retval = vfscanf(stream, format, args);
	va_end(args);
	return retval;
}

int sscanf(const char *str, const char *format, ...)
{
	//Set up va_list and call "v" version.
	va_list args;
	va_start(args, format);
	int retval = vsscanf(str, format, args);
	va_end(args);
	return retval;
}

int vscanf(const char *format, va_list ap)
{
	//Equivalent to calling vfscanf and specifying stdin as the stream.
	return vfscanf(stdin, format, ap);
}

int vdscanf(int fd, const char *format, va_list ap)
{
	//Phony FILE for reading directly from a file descriptor.
	FILE fd_stream = {
		.streamtype = _FILE_STREAMTYPE_RAWFD,
		.fd = fd,
	};	
	return vfscanf(&fd_stream, format, ap);
}

int dscanf(int fd, const char *format, ...)
{
	//Set up va_list and call "v" version.
	va_list args;
	va_start(args, format);
	int retval = vdscanf(fd, format, args);
	va_end(args);
	return retval;
}

int vsnscanf(const char *str, size_t size, const char *format, va_list ap)
{
	//Phony FILE for reading from a bounded buffer.
	FILE strn_stream = {
		.streamtype = _FILE_STREAMTYPE_STRN,
		.buf_ptr = (char*)str,
		.buf_size = size,
		.buf_out = 0,
	};
	return vfscanf(&strn_stream, format, ap);
}

int vsscanf(const char *str, const char *format, va_list ap)
{
	//Just do a vsnscanf with an enormous length.
	return vsnscanf(str, (INT_MAX)-1, format, ap);
}

int snscanf(const char *str, size_t size, const char *format, ...)
{
	//Set up va_list and call "v" version.
	va_list args;
	va_start(args, format);
	int retval = vsnscanf(str, size, format, args);
	va_end(args);
	return retval;
}
