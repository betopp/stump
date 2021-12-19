//file.h
//Table of open files in kernel
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef FILE_H
#define FILE_H

#include <sys/types.h>
#include <sys/stat.h>
#include "m_spl.h"

//Open file
typedef struct file_s
{
	m_spl_t spl; //Spinlock protecting the structure
	int refs; //Number of references to this file (0 means a free entry)
	
	int access; //Type of access opened for - reading, writing, or just stat (0)
	
	ino_t ino; //Inode we refer to
	off_t off; //File pointer - where in the file we will next read/write
	mode_t mode; //Mode of file at time of open
	dev_t special; //Special-number of file at time of open (pipe ID if this is a pipe, dev number if character/block dev)
	
} file_t;


//Makes a new file. Outputs a pointer to the new open file, with the lock held, and one reference.
int file_make(file_t *dir, const char *name, mode_t mode, dev_t special, file_t **file_out);

//Finds an existing file. Outputs a pointer to the new open file, with the lock held, and one reference.
//Pass NULL for "dir" to search the root directory. Pass empty-string as "name" to get a reference to the same file.
//Returns 0 on success or a negative error number.
int file_find(file_t *dir, const char *name, file_t **file_out);

//Unlinks a file from the given directory file.
int file_unlink(file_t *dir, const char *name, file_t *rmfile, int flags);

//Reads data from the open file.
ssize_t file_read(file_t *file, void *buf, ssize_t nbytes);

//Writes data into the open file.
ssize_t file_write(file_t *file, const void *buf, ssize_t nbytes);

//Changes the size of the given open file.
int file_trunc(file_t *file, off_t size);

//Returns status information about the given open file.
int file_stat(file_t *file, struct stat *st);

//Changes the file-pointer in the given open file, altering where the next read or write occurs.
off_t file_seek(file_t *file, off_t offset, int whence);

//Interface to device-specific functions on device specials
int file_ioctl(file_t *file, int operation, void *buf, ssize_t len);

//Acquires the lock on the given file.
void file_lock(file_t *file);

//Releases the lock on a file. Frees it if there are no references when unlocked.
void file_unlock(file_t *file);

#endif //FILE_H
