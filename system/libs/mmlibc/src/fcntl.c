//fcntl.c
//File descriptor functions for standard library
//Bryan E. Topp <betopp@betopp.com> 2021

#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include <sc.h>

//Bits masked-off from mode when creating new files.
//We stuff this in an environment variable so it persists across exec, as required by posix.
//It would be really silly to have the kernel concern itself with this - unless I'm missing something.
mode_t _umask_cached = ~0u;
void _umask_set(mode_t modeclr)
{
	//Set in environment and normal variable.
	_umask_cached = modeclr;
	char buf[16] = {0};
	snprintf(buf, 15, "0%o", modeclr);
	setenv("__sc_UMASK", buf, 1);
}
mode_t _umask_get()
{
	//Restore variable from environment if it's unset.
	if(_umask_cached == ~0u)
	{
		char *umask_str = getenv("__sc_UMASK");
		if(umask_str && *umask_str)
			_umask_cached = strtol(umask_str, NULL, 8);
		else
			_umask_cached = 022;
	}
	
	//Return from variable after looking up once.
	return _umask_cached;
}

mode_t umask(mode_t modeclr)
{
	mode_t old = _umask_get();
	_umask_set(modeclr);
	return old;
}

//Pathname resolution.
//Opens a directory file descriptor for the last nonfinal pathname component.
//Outputs the start of the final pathname component in *path_remain.
//Returns the file descriptor or a negative error number.
int _path(int fd, const char *path, const char **path_remain)
{
	//Check if we have an absolute path.
	//If so, consume the leading slashes, and do a relative resolution from the root dir.
	if(*path == '/')
	{
		while(*path == '/')
			path++;
		
		fd = _sc_find(-1, "/");
		if(fd < 0)
			return fd;
		
		int result = _path(fd, path, path_remain);
		_sc_close(fd);
		return result;
	}
	
	//Handle "cwd" case by finding "" in the working directory, which simply returns it.
	//Do a normal search starting there.
	if(fd == AT_FDCWD)
	{
		fd = _sc_find(-1, "");
		if(fd < 0)
			return fd;
		
		int result = _path(fd, path, path_remain);
		_sc_close(fd);
		return result;
	}
	
	//Okay, we've handled absolute and working-dir cases.
	//We now have a file descriptor to start with, and a relative path from it.
	
	//Make a copy of the caller's file descriptor to start.
	int work_fd = _sc_find(fd, "");
	if(work_fd < 0)
		return work_fd;
	
	//Todo - this needs to handle symlinks if requested.
	
	//Iterate through and look up all non-final pathname components.
	//These must exist in all cases - even if we're creating a file.
	while(strchr(path, '/') != NULL)
	{
		//We've already skipped leading slashes.
		//Copy out the pathname component, up to the next slash.
		char comp[256] = {0};
		size_t complen = 0;
		while(*path != '\0' && *path != '/' && complen < sizeof(comp))
		{
			comp[complen] = *path;
			path++;
			complen++;
		}
		
		if(complen >= sizeof(comp))
		{
			//Overlong pathname component (need at least one NUL in our comp buffer at the end)
			_sc_close(work_fd);
			return -ENAMETOOLONG;
		}
		
		//Try to find that pathname component in the previous directory.
		int next_fd = _sc_find(work_fd, comp);
		if(next_fd < 0)
		{
			//Failed to find the non-final pathname component
			_sc_close(work_fd);
			return next_fd;
		}
		
		//We no longer need our reference to the previous directory.
		//We'll look up following the component we just found.
		_sc_close(work_fd);
		work_fd = next_fd;
		
		//Advance past slashes, and then continue to examine the remaining path.
		while(*path == '/')
			path++;
	}
	
	//Alright, got the directory open.
	//Output the location of the final pathname component and return the directory.
	*path_remain = path;
	return work_fd;
}

//Non-variadic version of openat.
int _openatm(int fd, const char *path, int flags, mode_t mode)
{
	//Look up nonfinal pathname components.
	int work_fd = _path(fd, path, &path);
	if(work_fd < 0)
	{
		errno = -work_fd;
		return -1;
	}
		
	//Alright, we've resolved all but the last pathname component.
	//We have a file descriptor that references the directory where it should be made or found.
	//Try to get as much access as we can on that.
	int dir_access_err = _sc_access(work_fd, _SC_ACCESS_R | _SC_ACCESS_W | _SC_ACCESS_X, 0);
	(void)dir_access_err;
	
	//Try to create the file first, if creating it is permitted.
	//This will fail if it already exists, but we'll then try to open it.
	//If we tried to open the existing file first, we would have a race-condition with someone else also trying that.
	
	int create_result = -ENOSYS;
	if(flags & O_CREAT)
	{
		if( (flags & O_DIRECTORY) && !S_ISDIR(mode) )
		{
			//Don't attempt to make a non-directory if we were asked to only open a directory.
			errno = ENOTDIR;
			return -1;
		}
		
		//If they didn't specify a type of file, default to a "regular file" mode.
		if((mode & S_IFMT) == 0)
			mode |= S_IFREG;
		
		create_result = _sc_make(work_fd, path, mode & ~_umask_get(), 0);
	}
	
	//If we didn't create the file (or didn't try), and don't want only to create the file, try getting the existing one.
	int find_result = -ENOSYS;
	if((create_result < 0) && !((flags & O_CREAT) && (flags & O_EXCL)))
	{
		find_result = _sc_find(work_fd, path);
	}
	
	//Whether successful or not, we're done with the directory.
	_sc_close(work_fd);
	work_fd = -1;

	if(create_result < 0 && find_result < 0)
	{
		if(create_result != -ENOSYS)
			errno = -create_result; //maybe? how do we pick?
		else
			errno = -find_result;
		
		return -1; //Could neither open nor create this file.
	}
	
	//Alright, we've got the file open.
	int fd_ret = -1;
	if(create_result > 0)
	{
		assert(find_result < 0);
		fd_ret = create_result;
	}
	else
	{
		assert(find_result >= 0);
		fd_ret = find_result;
	}
	
	//If we only wanted to open a directory, make sure it is.
	if(flags & O_DIRECTORY)
	{
		_sc_stat_t st = {0};
		int st_err = _sc_stat(fd_ret, &st, sizeof(st));
		if(st_err < 0)
		{
			//Failed to stat the file we just opened...?
			errno = -st_err;
			_sc_close(fd_ret);
			fd_ret = -1;
		}
		
		if(!S_ISDIR(st.mode))
		{
			//Wanted to only open a directory, but this isn't one.
			errno = ENOTDIR;
			_sc_close(fd_ret);
			fd_ret = -1;
		}
	}
	
	//If we wanted to append, go to the end of the file
	if(flags & O_APPEND)
	{
		_sc_seek(fd_ret, 0, SEEK_END);
	}
	
	//The kernel defaults to close-on-exec, with a flag for keep-through-exec.
	//This is the opposite of the conventional behavior. Set keep-through-exec unless asked for close-on-exec.
	if((fd_ret >= 0) && !(flags & O_CLOEXEC))
		_sc_flag(fd_ret, _SC_FLAG_KEEPEXEC, 0);
			
	int access_desired = 0;
	if(flags & O_RDONLY)
		access_desired |= _SC_ACCESS_R;
	if(flags & O_EXEC)
		access_desired |= _SC_ACCESS_X;
	if(flags & O_WRONLY)
		access_desired |= _SC_ACCESS_W;
	if(flags & O_RDWR)
		access_desired |= _SC_ACCESS_W | _SC_ACCESS_R; //Not stricly necessary since RDWR == RDONLY | WRONLY
	
	int access_err = _sc_access(fd_ret, access_desired, 0);
	if(access_err < 0)
	{
		errno = -access_err;
		_sc_close(fd_ret);
		fd_ret = -1;
	}
	
	return fd_ret;
}

//Non-variadic version of open.
int _openm(const char *path, int flags, mode_t mode)
{
	//open is equivalent to openat with fd of AT_FDCWD
	return _openatm(AT_FDCWD, path, flags, mode);
}

int open(const char *path, int flags, ...)
{
	//Kill variadic parameter and call non-variadic version.
	mode_t mode = 0;
	if(flags & O_CREAT)
	{
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	
	return _openm(path, flags, mode);
}

int openat(int fd, const char *path, int flags, ...)
{
	//Kill variadic parameter and call non-variadic version.
	mode_t mode = 0;
	if(flags & O_CREAT)
	{
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	
	return _openatm(fd, path, flags, mode);
}

ssize_t write(int fd, const void *buf, size_t nbytes)
{
	ssize_t result = _sc_write(fd, buf, nbytes);
	if(result < 0)
	{
		errno = -result;
		return -1;
	}
	return result;
}

ssize_t read(int fd, void *buf, size_t nbytes)
{
	ssize_t result = _sc_read(fd, buf, nbytes);
	if(result < 0)
	{
		errno = -result;
		return -1;
	}
	return result;
}


int dup(int oldfd)
{
	int result = _sc_dup(oldfd, 0, false);
	if(result < 0)
	{
		errno = -result;
		return -1;
	}
	_sc_flag(result, _SC_FLAG_KEEPEXEC, 0);
	return result;
}

int dup2(int oldfd, int newfd)
{
	int result = _sc_dup(oldfd, newfd, true);
	if(result < 0)
	{
		errno = -result;
		return -1;
	}
	assert(result == newfd);
	_sc_flag(result, _SC_FLAG_KEEPEXEC, 0);
	return result;
}

int fcntl(int fd, int cmd, ...)
{
	//Check command and resolve third parameter.
	int arg_int = 0;
	void *arg_ptr = NULL;
	va_list ap;
	switch(cmd)
	{
		case F_DUPFD:
		case F_DUPFD_CLOEXEC:
		case F_SETFD:		
		case F_SETFL:	
		case F_SETOWN:			
			va_start(ap, cmd);
			arg_int = va_arg(ap, int);
			va_end(ap);
			break;
		
		case F_GETLK:
		case F_SETLK:
		case F_SETLKW:
			va_start(ap, cmd);
			arg_ptr = va_arg(ap, void*);
			va_end(ap);
			break;
		
		case F_GETOWN:
		case F_GETFL:
		case F_GETFD:
			break;
		
		default:
			errno = EINVAL;
			return -1;
	}
	
	//Act on command
	switch(cmd)
	{
		case F_DUPFD:
		{
			int result = _sc_dup(fd, arg_int, false);
			if(result < 0)
			{
				errno = -result;
				return -1;
			}
			//Set keep-through-exec flag if we weren't asked to close-on-exec.
			_sc_flag(result, _SC_FLAG_KEEPEXEC, 0);
			return result;
		}
		case F_DUPFD_CLOEXEC:
		{
			int dup_result = _sc_dup(fd, arg_int, false);
			if(dup_result < 0)
			{
				errno = -dup_result;
				return -1;
			}
			//Asked to close-on-exec - don't set keep-through-exec flag.
			return dup_result;
		}
		case F_GETFD:
		{
			int k_flags = _sc_flag(fd, 0, 0);
			if(k_flags < 0)
			{
				errno = -k_flags;
				return -1;
			}
			
			if(k_flags & _SC_FLAG_KEEPEXEC)
				return 0;
			else
				return FD_CLOEXEC;
		}
		case F_SETFD:
		{
			int result = 0;
			if(arg_int & FD_CLOEXEC)
				result = _sc_flag(fd, 0, _SC_FLAG_KEEPEXEC); //Close-on-exec wanted - Clear keep-on-exec
			else
				result = _sc_flag(fd, _SC_FLAG_KEEPEXEC, 0); //Close-on-exec not wanted - set keep-on-exec.
			
			if(result < 0)
			{
				errno = -result;
				return -1;
			}
			return result;
		}
		default:
			(void)arg_ptr;
			errno = -EINVAL;
			return -1;
	}
}

off_t lseek(int fd, off_t off, int whence)
{
	off_t result = _sc_seek(fd, off, whence);
	if(result < 0)
	{
		errno = -result;
		return -1;
	}
	return result;
}

int close(int fd)
{
	int result = _sc_close(fd);
	if(result < 0)
	{
		errno = -result;
		return -1;
	}
	return 0;
}

int fstat(int fd, struct stat *out)
{
	//Performing a stat on an open file is the way our kernel works natively.
	_sc_stat_t _sc_stat_buf = {0};
	int stat_err = _sc_stat(fd, &_sc_stat_buf, sizeof(_sc_stat_buf));
	if(stat_err < 0)
	{
		errno = -stat_err;
		return -1;
	}
	
	//Convert kernel stat struct to libc struct.
	//Todo - is it worth defining these to be the same?
	//Seems like there might be reasons not to, but I can't come up with any.
	memset(out, 0, sizeof(*out));
	out->st_dev = _sc_stat_buf.dev;
	out->st_ino = _sc_stat_buf.ino;
	out->st_mode = _sc_stat_buf.mode;
	out->st_size = _sc_stat_buf.size;
	out->st_rdev = _sc_stat_buf.rdev;
	
	return 0;
}

int fstatat(int at_fd, const char *path, struct stat *out, int flag)
{
	//Our kernel has the concept of "open for stat", without needing permission for read nor write nor exec.
	//Try to open the given path for stat.
	int oflag = O_STAT | O_CLOEXEC;
	if(flag & AT_SYMLINK_NOFOLLOW)
		oflag |= O_NOFOLLOW;
	
	int fd = _openatm(at_fd, path, oflag, 0);
	if(fd < 0)
		return -1; //_openatm sets errno
	
	//Got the file open for stat. Stat it.
	int fstat_err = fstat(fd, out); //can set errno on failure

	//Either success or failure, just close the file and return what happened.
	_sc_close(fd);
	return fstat_err;
}

int stat(const char *path, struct stat *out)
{
	return fstatat(AT_FDCWD, path, out, 0);
}

int lstat(const char *path, struct stat *out)
{
	return fstatat(AT_FDCWD, path, out, AT_SYMLINK_NOFOLLOW);
}

ssize_t readlinkat(int at_fd, const char *path, char *buf, size_t len)
{
	int fd = _openatm(at_fd, path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW, 0);
	if(fd < 0)
		return -1; //_openatm sets errno
	
	ssize_t read_result = _sc_read(fd, buf, len);
	_sc_close(fd);
	
	if(read_result < 0)
	{
		errno = -read_result;
		return -1;
	}
	return read_result;
}

ssize_t readlink(const char *path, char *buf, size_t len)
{
	return readlinkat(AT_FDCWD, path, buf, len);
}

int fchdir(int fd)
{
	//Make sure this is a directory.
	//The kernel will let us retain whatever file we want as our current directory,
	//but everything will start to fail with ENOTDIR in that case.
	_sc_stat_t st = {0};
	ssize_t stat_err = _sc_stat(fd, &st, sizeof(st));
	if(stat_err < 0)
	{
		errno = -stat_err;
		return -1;
	}
	
	if(!S_ISDIR(st.mode))
	{
		errno = ENOTDIR;
		return -1;
	}
	
	int result = _sc_chdir(fd);
	if(result < 0)
	{
		errno = -result;
		return -1;
	}
	return 0;
}

int chdir(const char *path)
{
	//Process needs exec permission on a directory to change to it. Try to open the path for exec.
	int fd = _openatm(AT_FDCWD, path, O_EXEC | O_CLOEXEC | O_RDWR, 0);
	if(fd < 0)
		return -1; //_openatm sets errno
	
	int result = fchdir(fd);
	_sc_close(fd);
	return result;
}

int ftruncate(int fd, off_t length)
{
	int err = _sc_trunc(fd, length);
	if(err < 0)
	{
		errno = -err;
		return -1;
	}
	return 0;
}

int truncate(const char *path, off_t length)
{
	int fd = _openatm(AT_FDCWD, path, O_WRONLY | O_CLOEXEC, 0);
	if(fd < 0)
		return -1; //_openatm sets errno
	
	int err = _sc_trunc(fd, length);
	if(err < 0)
	{
		errno = -err;
		return -1;
	}
	
	_sc_close(fd);
	return 0;
}

//This is a recent and nonstandard addition from the FreeBSD API. 
//It solves an important problem though ("unlink exactly this file I've been looking at") so we copy it.
int funlinkat(int dfd, const char *path, int fd, int flag)
{
	//Work through nonfinal pathname components and a get a descriptor for the final directory.
	int work_fd = _path(dfd, path, &path);
	if(work_fd < 0)
	{
		errno = -work_fd;
		return -1;
	}	
	
	int access_err = _sc_access(work_fd, _SC_ACCESS_W, 0);
	if(access_err < 0)
	{
		//Don't have permission on this directory
		errno = -access_err;
		return -1;
	}
	
	//We now have the final pathname component and the directory that actually contains it.
	//Try to remove it.
	int result = _sc_unlink(work_fd, path, fd, flag);
	
	//Close the directory descriptor, regardless
	_sc_close(work_fd);
	
	if(result < 0)
	{
		errno = -result;
		return -1;
	}

	return 0;
}

int unlinkat(int dfd, const char *path, int flag)
{
	return funlinkat(dfd, path, -1, flag);
}

int unlink(const char *path)
{
	return unlinkat(AT_FDCWD, path, 0);
}

int rmdir(const char *path)
{
	return unlinkat(AT_FDCWD, path, AT_REMOVEDIR);
}

int faccessat(int fd, const char *path, int mode, int flag)
{
	int testfd = _openatm(fd, path, flag | O_CLOEXEC, 0);
	if(testfd < 0)
		return -1; //_openatm sets error
	
	int access_err = _sc_access(testfd, mode, 0);
	_sc_close(testfd); //Close regardless of access result
	
	if(access_err < 0)
	{
		//Couldn't set requested access
		errno = -access_err;
		return -1;
	}
	
	return 0;
}

int access(const char *path, int mode)
{
	return faccessat(AT_FDCWD, path, mode, 0);
}

int eaccess(const char *path, int mode)
{
	return faccessat(AT_FDCWD, path, mode, AT_EACCESS);
}

