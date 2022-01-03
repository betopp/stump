//stdio.c
//Buffered IO implementation
//Bryan E. Topp <betopp@betopp.com> 2021

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

//Buffers for stdin/stdout/stderr streams
static char _stdin_buffer[512];
static char _stdout_buffer[512];
static char _stderr_buffer[512];

//Stream information for stdin/stdout/stderr streams
FILE _stdin_storage = {
	.streamtype = _FILE_STREAMTYPE_BUFFD,
	.fd = STDIN_FILENO,
	.omode = O_RDONLY,
	.buf_ptr = _stdin_buffer,
	.buf_size = sizeof(_stdin_buffer),
	.buf_out = 0,
	.buf_mode = _IOLBF,
	.buf_ext = 1,
};

FILE _stdout_storage = {
	.streamtype = _FILE_STREAMTYPE_BUFFD,
	.fd = STDOUT_FILENO,
	.omode = O_WRONLY,
	.buf_ptr = _stdout_buffer,
	.buf_size = sizeof(_stdout_buffer),
	.buf_out = 1,
	.buf_mode = _IOLBF,
	.buf_ext = 1,
};

FILE _stderr_storage = {
	.streamtype = _FILE_STREAMTYPE_BUFFD,
	.fd = STDERR_FILENO,
	.omode = O_WRONLY,
	.buf_ptr = _stderr_buffer,
	.buf_size = sizeof(_stderr_buffer),
	.buf_out = 1,
	.buf_mode = _IOLBF,
	.buf_ext = 1,
};

//Converts a mode string (e.g. from fopen) to flags that we can pass to open().
static int _oflags_for_modestring(const char *mode)
{	
	if( (strcmp(mode, "r") == 0) || (strcmp(mode, "rb") == 0) )
	{
		return O_RDONLY;
	}
	
	if( (strcmp(mode, "w") == 0) || (strcmp(mode, "wb") == 0) )
	{
		return O_WRONLY | O_TRUNC | O_CREAT;
	}
	
	if( (strcmp(mode, "a") == 0) || (strcmp(mode, "ab") == 0) )
	{
		return O_WRONLY | O_APPEND | O_CREAT;
	}
	
	if( (strcmp(mode, "r+") == 0) || (strcmp(mode, "rb+") == 0) || (strcmp(mode, "r+b") == 0) )
	{
		return O_RDWR;
	}
	
	if( (strcmp(mode, "w+") == 0) || (strcmp(mode, "wb+") == 0) || (strcmp(mode, "w+b") == 0) )
	{
		return O_RDWR | O_TRUNC | O_CREAT;
	}
	
	if( (strcmp(mode, "a+") == 0) || (strcmp(mode, "ab+") == 0) || (strcmp(mode, "a+b") == 0) )
	{
		return O_RDWR | O_APPEND | O_CREAT;
	}
	
	//Other strings passed to mode cause "undefined behavior".
	assert(0);
	return O_RDONLY;
}

//Implementation of fgetc - for normal, buffered-file-descripor streams.
static int _fgetc_buffd(FILE *stream)
{
	assert(stream->streamtype == _FILE_STREAMTYPE_BUFFD);
	
	//If there's data buffered for output, we need to write it out before using the buffer for reading.
	if(stream->buf_out)
	{
		//Make writes until we've written-out all data waiting to be written.
		while(stream->buf_rpos < stream->buf_wpos)
		{
			ssize_t written_next = write(stream->fd, stream->buf_ptr + stream->buf_rpos, stream->buf_wpos - stream->buf_rpos);
			if(written_next < 0)
			{
				//Error while flushing write buffer
				stream->error = -written_next;
				return EOF;
			}
			stream->buf_rpos += written_next;
		}
		
		//Flushed all waiting output
		stream->buf_out = 0;
		stream->buf_wpos = 0;
		stream->buf_rpos = 0;
	}
	
	//If the buffer is empty, fill it
	if(stream->buf_rpos >= stream->buf_wpos)
	{
		//Reset the buffer, as all its contents have been consumed
		assert(stream->buf_rpos == stream->buf_wpos);
		stream->buf_rpos = 0;
		stream->buf_wpos = 0;
		
		//Try a read and see how much we can fill into the buffer
		ssize_t read_bytes = read(stream->fd, stream->buf_ptr, stream->buf_size);
		if(read_bytes < 0)
		{
			//Error reading into the buffer
			stream->error = 1;
			return EOF;
		}
		
		if(read_bytes == 0)
		{
			//No more data
			stream->eof = 1;
			return EOF;
		}
		
		//Note that we have data in the buffer now
		stream->buf_wpos = read_bytes;
		return 0;
	}
	
	//Should have written some input data into the buffer, and now can read it out
	assert(stream->buf_rpos < stream->buf_wpos);
	int nextchar = stream->buf_ptr[stream->buf_rpos];
	stream->buf_rpos++;
	return nextchar;
}

//Implementation of fgetc - for raw, unbuffered file descriptor streams
static int _fgetc_rawfd(FILE *stream)
{
	assert(stream->streamtype == _FILE_STREAMTYPE_RAWFD);
	
	unsigned char charbuf = 0;
	ssize_t readval = read(stream->fd, &charbuf, 1);
	if(readval == 1)
	{
		//Success
		return charbuf;
	}
	else
	{
		//Failure
		stream->error = 1;
		return EOF;
	}
}

//Implementation of fgetc - for someone else's buffer in memory
static int _fgetc_strn(FILE *stream)
{
	assert(stream->streamtype == _FILE_STREAMTYPE_STRN);
	
	if(stream->buf_rpos >= stream->buf_size)
	{
		stream->eof = 1;
		return EOF;
	}
	
	int retval = stream->buf_ptr[stream->buf_rpos];
	stream->buf_rpos++;
	stream->buf_wpos = stream->buf_rpos;
	return retval;
}

//Drains an output buffer to its backing file.
static int _fputc_drainfd(FILE *stream)
{
	assert(stream->buf_out);
	assert(stream->buf_wpos <= stream->buf_size);
	while(stream->buf_rpos < stream->buf_wpos)
	{
		ssize_t write_bytes = write(stream->fd, stream->buf_ptr + stream->buf_rpos, stream->buf_wpos - stream->buf_rpos);
		if(write_bytes <= 0)
		{
			//Error flushing output buffer
			stream->error = 1;
			return EOF;
		}
		stream->buf_rpos += write_bytes;
	}
	
	//Buffer has been written out; reset it.
	stream->buf_rpos = 0;
	stream->buf_wpos = 0;
	
	return 0;
}

//Implementation of fputc - for normal, buffered-file-descripor streams.
static int _fputc_buffd(int c, FILE *stream)
{
	assert(stream->streamtype == _FILE_STREAMTYPE_BUFFD);
	
	//Ditch any input that's waiting, so the buffer can be used for output
	if(!stream->buf_out)
	{
		//We might have some input in the buffer that we read from the file.
		//Some of that might have already been consumed by the application.
		//Any that's been read-in from the file, but not consumed, will need to get read again.
		assert(stream->buf_rpos <= stream->buf_wpos);
		lseek(stream->fd, -(stream->buf_wpos - stream->buf_rpos), SEEK_CUR);
		
		stream->buf_out = 1;
		stream->buf_wpos = 0;
		stream->buf_rpos = 0;
	}
	
	//If the output buffer is full, drain it
	if(stream->buf_wpos >= stream->buf_size)
	{
		if(_fputc_drainfd(stream) == EOF)
			return EOF;
	}
	
	//Put the character in the buffer, now that it's used for output and there's room
	stream->buf_ptr[stream->buf_wpos] = c;
	stream->buf_wpos++;
	
	//If we're unbuffered, always drain after writing.
	//If we're linebuffered, drain after writing a newline.
	if((c == '\n' && stream->buf_mode == _IOLBF) || (stream->buf_mode == _IONBF))
	{
		if(_fputc_drainfd(stream) == EOF)
			return EOF;
	}
	
	return (unsigned char)c;
}

//Implementation of fputc - for raw, unbuffered file descriptor streams.
static int _fputc_rawfd(int c, FILE *stream)
{
	assert(stream->streamtype == _FILE_STREAMTYPE_RAWFD);
	
	unsigned char c_char = c;
	ssize_t written = write(stream->fd, &c_char, 1);
	if(written == 1)
	{
		//Success
		return 0;
	}
	else
	{
		//Error
		stream->error = 1;
		return EOF;
	}
}

//Implementation of fputc - for someone else's buffer in memory
static int _fputc_strn(int c, FILE *stream)
{
	assert(stream->streamtype == _FILE_STREAMTYPE_STRN);
	
	if(stream->buf_wpos >= stream->buf_size)
	{
		stream->eof = 1;
		return EOF;
	}
	
	stream->buf_ptr[stream->buf_wpos] = c;
	stream->buf_wpos++;
	stream->buf_rpos = stream->buf_wpos;
	return c;
}

long ftell(FILE *stream)
{
	long fd_pos = 0;
	if(stream->streamtype == _FILE_STREAMTYPE_RAWFD || stream->streamtype == _FILE_STREAMTYPE_BUFFD)
	{
		fd_pos = lseek(stream->fd, 0, SEEK_CUR);
		if(fd_pos < 0)
		{
			stream->error = 1;
			errno = -fd_pos;
			return -1;
		}
	}
	
	long buf_off = 0;
	if(stream->buf_out)
		buf_off = stream->buf_wpos - stream->buf_rpos; //User has written bytes, ahead of FD's position
	else
		buf_off = -(stream->buf_wpos - stream->buf_rpos); //FD was read for the user in advance
	
	return fd_pos + buf_off;
}

int fflush(FILE *stream)
{
	if(stream->streamtype != _FILE_STREAMTYPE_BUFFD)
	{
		//Not a buffered stream backed by a file descriptor.
		//Nothing to flush.
		return 0;
	}
	
	if(!stream->buf_out)
	{
		//Last operation was not output; nothing to flush.
		return 0;
	}
	
	while(stream->buf_rpos < stream->buf_wpos)
	{
		ssize_t write_bytes = write(stream->fd, stream->buf_ptr + stream->buf_rpos, stream->buf_wpos - stream->buf_rpos);
		if(write_bytes <= 0)
		{
			//Error flushing output buffer
			stream->error = 1;
			return EOF;
		}
		stream->buf_rpos += write_bytes;
	}
	
	//Buffer flushed
	stream->buf_rpos = 0;
	stream->buf_wpos = 0;
	return 0;
}


int setvbuf(FILE *stream, char *buf, int mode, size_t size)
{
	//Validate buffer mode
	if(mode != _IONBF && mode != _IOLBF && mode != _IOFBF)
	{
		errno = EINVAL;
		return EOF;
	}
	
	//If they asked for a nonzero buffer size and didn't provde the buffer, we allocate it.
	//(The caller is being nonstandard but this is how FreeBSD works.)
	int alloc_here = 0;
	if(buf == NULL && size > 0)
	{
		buf = malloc(size);
		if(buf == NULL)
			return EOF;
		
		alloc_here = 1;
	}
	
	if(stream->buf_ptr && !(stream->buf_ext))
		free(stream->buf_ptr);
	
	stream->buf_ptr = buf;
	stream->buf_size = size;
	stream->buf_rpos = 0;
	stream->buf_wpos = 0;
	
	stream->buf_mode = mode;
	stream->buf_ext = !alloc_here;
	
	return 0;
}

void setbuf(FILE *stream, char *buf)
{
	//Definition from the FreeBSD manual
	setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

//Implementation of fseek - for normal, buffered-file-descripor streams.
static int _fseeko_buffd(FILE *stream, off_t offset, int whence)
{
	//If there's data waiting to be written, flush it before seeking
	fflush(stream);
	
	//Reset buffer totally
	stream->buf_rpos = 0;
	stream->buf_wpos = 0;
	
	//Seek the underlying file
	off_t fdseek = lseek(stream->fd, offset, whence);
	return fdseek;
}

//Implementation of fseek - for raw, unbuffered file descriptor streams.
static int _fseeko_rawfd(FILE *stream, off_t offset, int whence)
{
	return lseek(stream->fd, offset, whence);
}

//Implementation of fseek - for someone else's buffer in memory
static int _fseeko_strn(FILE *stream, off_t offset, int whence)
{
	switch(whence)
	{
		case SEEK_SET:
			stream->buf_rpos = offset;
			break;
		case SEEK_CUR:
			stream->buf_rpos += offset;
			break;
		case SEEK_END:
			stream->buf_rpos = stream->buf_size + offset;
			break;
		default:
			//Bad "whence" parameter
			errno = EINVAL;
			return -1;
	}
	
	if(stream->buf_rpos < 0)
		stream->buf_rpos = 0;
	if(stream->buf_rpos > stream->buf_size)
		stream->buf_rpos = stream->buf_size;
	
	stream->buf_wpos = stream->buf_rpos;
	return stream->buf_rpos;
}

int ungetc(int ch, FILE *stream)
{
	if(ch == EOF)
		return EOF;
	
	stream->ungetc_present = 1;
	stream->ungetc_value = ch;
	stream->eof = 0;
	return ch;
}

int fgetc(FILE *stream)
{	
	if(stream->ungetc_present)
	{
		stream->ungetc_present = 0;
		return stream->ungetc_value;
	}
	
	switch(stream->streamtype)
	{
		case _FILE_STREAMTYPE_BUFFD:
			return _fgetc_buffd(stream);
		case _FILE_STREAMTYPE_RAWFD:
			return _fgetc_rawfd(stream);
		case _FILE_STREAMTYPE_STRN:
			return _fgetc_strn(stream);
		default:
			assert(0);
			errno = EBADF;
			return EOF;
	}
}

int fputc(int c, FILE *stream)
{
	switch(stream->streamtype)
	{
		case _FILE_STREAMTYPE_BUFFD:
			return _fputc_buffd(c, stream);
		case _FILE_STREAMTYPE_RAWFD:
			return _fputc_rawfd(c, stream);
		case _FILE_STREAMTYPE_STRN:
			return _fputc_strn(c, stream);
		default:
			assert(0);
			errno = EBADF;
			return EOF;
	}
}

int fseeko(FILE *stream, off_t offset, int whence)
{
	switch(stream->streamtype)
	{
		case _FILE_STREAMTYPE_BUFFD:
			return _fseeko_buffd(stream, offset, whence);
		case _FILE_STREAMTYPE_RAWFD:
			return _fseeko_rawfd(stream, offset, whence);
		case _FILE_STREAMTYPE_STRN:
			return _fseeko_strn(stream, offset, whence);
		default:
			assert(0);
			errno = EBADF;
			return -1;
	}
}

int fseek(FILE *stream, long offset, int whence)
{
	return fseeko(stream, offset, whence);
}

int getc(FILE *stream)
{
	return fgetc(stream);
}

int getchar(void)
{
	return getc(stdin);
}

int putc(int c, FILE *stream)
{
	return fputc(c, stream);
}

int putchar(int c)
{
	return putc(c, stdout);
}

int puts(const char *s)
{
	int string_puts_val = fputs(s, stdout);
	if(string_puts_val < 0)
		return EOF;
	
	int newline_putc_val = fputc('\n', stdout);
	if(newline_putc_val < 0)
		return EOF;
	
	return 0;	
}

int fputs(const char *s, FILE *stream)
{
	while(*s != '\0')
	{
		if(fputc(*s, stream) == EOF)
			return EOF;
		
		s++;
	}
	
	return 0;
}

char *fgets(char *s, int size, FILE *stream)
{
	for(int cc = 0; cc < (size - 1); cc++)
	{
		int nextchar = fgetc(stream);
		if(nextchar == EOF)
		{
			//No more characters - just store a NUL.
			s[cc] = '\0';
			
			//Return value depends on whether any characters were successfully read.
			if(cc == 0)
				return NULL; //Error or EOF with no characters read.
			else
				return s; //Error or EOF, but we've read some characters.
		}
		else if(nextchar == '\n')
		{
			//Got a newline. Store and stop reading.
			s[cc] = nextchar;
			s[cc+1] = '\0';
			return s;
		}
		else
		{
			//Got a character, but it's not a newline. Store and continue.
			s[cc] = nextchar;
			s[cc+1] = '\0';
		}
	}
	
	//Read as many characters as requested
	return s;
}

void rewind(FILE *stream)
{
	//Linux says that rewind is equivalent to this:
	(void)fseek(stream, 0L, SEEK_SET);
	stream->error = 0;
}

void clearerr(FILE *stream)
{
	stream->eof = 0;
	stream->error = 0;
}

int feof(FILE *stream)
{
	return stream->eof;
}

int ferror(FILE *stream)
{
	return stream->error;
}

int fileno(FILE *stream)
{
	return stream->fd;
}

FILE *fopen(const char *path, const char *mode)
{
	//Convert the textual mode to a mode number.
	int oflags = _oflags_for_modestring(mode);
	
	//Try to open the underlying file.
	int fd = open(path, oflags, 0666);
	if(fd < 0)
	{
		//Failed to open the underlying file.
		//errno set by open.
		return NULL;
	}
	
	//Try to open a stream on it.
	FILE *retval = fdopen(fd, mode);
	if(retval == NULL)
	{
		//Error making stream info.
		//errno set by fdopen.
		close(fd);
		return NULL;
	}
	
	//Success
	return retval;
}

FILE *fdopen(int fd, const char *mode)
{	
	//Allocate space for the file structure
	FILE *retval = malloc(sizeof(FILE));
	if(retval == NULL)
	{
		//No room for file structure.
		errno = ENOMEM;
		return NULL;
	}
	
	//Note open-mode
	retval->omode = _oflags_for_modestring(mode);
	
	//Allocate buffer
	retval->buf_size = BUFSIZ;
	retval->buf_rpos = 0;
	retval->buf_wpos = 0;
	retval->buf_ptr = malloc(retval->buf_size);
	if(retval->buf_ptr == NULL)
	{
		//No room for buffer.
		errno = ENOMEM;
		free(retval);
		return NULL;
	}
	retval->buf_ext = 0; //We free buf_ptr
	
	//Note file descriptor backing the stream.
	retval->streamtype = _FILE_STREAMTYPE_BUFFD;
	retval->fd = fd;
	
	//Success
	return retval;
}

int fclose(FILE *stream)
{
	//Flush the stream being closed
	int retval = 0;
	int flushval = fflush(stream);
	if(flushval == EOF)
	{
		//Error flushing... fflush sets errno.
		//We're still supposed to continue cleaning up, though.
		retval = EOF;
	}
	
	//If there's an underlying file descriptor, close it.
	if(stream->fd >= 0)
	{
		close(stream->fd);
		stream->fd = -1;
	}
	
	//Free the buffer if we own it
	if(stream->streamtype != _FILE_STREAMTYPE_STRN)
	{
		if(stream->buf_ptr != NULL && !(stream->buf_ext))
		{
			free(stream->buf_ptr);
			stream->buf_ptr = NULL;
		}
	}
	
	//Free the structure
	free(stream);
	
	return retval;
}

size_t fread(void *ptr, size_t size, size_t nitems, FILE *stream)
{
	if(size == 0 || nitems == 0)
	{
		return 0;
	}
	
	if(stream == NULL)
	{
		errno = EBADF;
		return 0;
	}
	
	//If we have data buffered for write, flush it, and indicate that the buffer is used for reading.
	if(stream->buf_out)
	{
		int flushresult = fflush(stream);
		if(flushresult != 0)
		{
			//fflush will have set errno
			return 0;
		}
		
		stream->buf_out = false;
	}
	
	//Read until we've satisfied the number of bytes requested, or encountered an error or EOF.
	unsigned char *outbuf = (unsigned char*)ptr;
	size_t bytesread = 0;
	while(bytesread < size*nitems)
	{
		//Fill with an ungetc'd character if any
		if(stream->ungetc_present)
		{
			outbuf[bytesread] = stream->ungetc_value;
			bytesread++;
			
			stream->ungetc_present = false;
			stream->ungetc_value = 0;
			
			continue;
		}
			
		//Drain bytes from the stream buffer into the output
		assert(stream->buf_rpos <= stream->buf_wpos);
		size_t buf_remain = stream->buf_wpos - stream->buf_rpos;
		if(buf_remain > 0)
		{
			size_t copysize = buf_remain;
			size_t bytesremain = (size*nitems) - bytesread;
			if(copysize > bytesremain)
				copysize = bytesremain;
		
			memcpy(outbuf + bytesread, stream->buf_ptr + stream->buf_rpos, copysize);
			stream->buf_rpos += copysize;
			bytesread += copysize;
			
			continue;
		}

		//If we couldn't return an ungetc'd character or the existing buffer, we need to go to the backing file.
		//If we don't have a backing file then we're done.
		if(stream->fd < 0)
		{
			stream->eof = true;
			break;
		}

	
		//Otherwise, try to read from the backing file.
		if(stream->buf_size > 0)
		{
			//We already established, we've read through the whole buffer...
			//So clear it out.
			assert(stream->buf_rpos == stream->buf_wpos);
			memset(stream->buf_ptr, 0, stream->buf_size);
			stream->buf_wpos = 0;
			stream->buf_rpos = 0;
			
			//Try to fill the buffer with data from the backing file.
			ssize_t readresult = read(stream->fd, stream->buf_ptr, stream->buf_size);
			if(readresult > 0)
			{
				//Got some data - use it to initialize the stream's buffer.
				//We'll come back around and drain the buffer in the next outer-loop.
				stream->buf_wpos = readresult;
			}
			else if(readresult == 0)
			{
				//Encountered EOF.
				stream->eof = true;
				break;
			}
			else
			{
				//Encountered error. read() should have set errno.
				assert(readresult == -1);
				stream->error = true;
				break;
			}	
		}
		else
		{
			//No buffer - try to read directly from the backing file to the output buffer.
			ssize_t readresult = read(stream->fd, outbuf + bytesread, (size*nitems) - bytesread);
			if(readresult > 0)
			{
				//Got some data, can keep reading
				bytesread += readresult;
			}
			else if(readresult == 0)
			{
				//Encountered EOF.
				stream->eof = true;
				break;
			}
			else
			{
				//Encountered error. read() should have set errno.
				assert(readresult == -1);
				stream->error = true;
				break;
			}
			
		}		
	}
	
	//Shouldn't have read more than we asked for...
	assert(bytesread <= size*nitems);
	return (bytesread / size);
}

size_t fwrite(const void *ptr, size_t size, size_t nitems, FILE *stream)
{
	if(size == 0 || nitems == 0)
	{
		return 0;
	}
	
	//If we have data buffered for read, flush it, and indicate that the buffer is used for writing.
	if(!stream->buf_out)
	{
		int flushresult = fflush(stream);
		if(flushresult != 0)
		{
			//fflush will have set errno
			return 0;
		}

		stream->buf_out = true;
	}
	
	//Write until we've covered the requested size, or encountered an error or EOF.
	const unsigned char *inbuf = (const unsigned char*)ptr;
	size_t byteswritten = 0;
	while(byteswritten < size * nitems)
	{
		//Try to write the output data into the stream's buffer
		assert(stream->buf_wpos <= stream->buf_size);
		size_t buf_remain = stream->buf_size - stream->buf_wpos;
		if(buf_remain > 0)
		{
			//Room to write in buffer. Copy the data in.
			size_t copysize = buf_remain;
			size_t bytesremain = (size*nitems) - byteswritten;
			if(copysize > bytesremain)
				copysize = bytesremain;
			
			memcpy(stream->buf_ptr + stream->buf_wpos, inbuf + byteswritten, copysize);
			stream->buf_wpos += copysize;
			byteswritten += copysize;
			
			continue;
		}
		
		//If we filled the buffer with our written data, we need to flush it to the backing file to make more room.
		//If there's no backing file, we're at end-of-file once the buffer is full.
		if(stream->fd < 0)
		{
			stream->eof = true;
			break;
		}
		
		//Otherwise, try writing to the backing file
		if(stream->buf_size > 0)
		{
			//We've actually got a buffer. Try to flush it.
			int flushresult = fflush(stream);
			if(flushresult != 0)
			{
				//fflush will have set errno and error indicator
				break;
			}
			
			//We should now have a clean buffer
			assert(stream->buf_rpos == 0);
			assert(stream->buf_wpos == 0);
		}
		else
		{
			//No buffer... try to write our data directly.
			ssize_t writeresult = write(stream->fd, inbuf + byteswritten, (size*nitems) - byteswritten);
			if(writeresult > 0)
			{
				//Managed to write some, keep going
				byteswritten += writeresult;
			}
			else if(writeresult == 0)
			{
				//Encountered EOF.
				stream->eof = true;
				break;
			}
			else
			{
				//Encountered error. write() should have set errno.
				assert(writeresult == -1);
				stream->error = true;
				break;
			}
		}
	}
	
	//Shouldn't have written more than asked for
	assert(byteswritten <= size * nitems);
	return (byteswritten / size);
}
