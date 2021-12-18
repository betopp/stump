//ramfs.h
//In-memory filesystem
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef RAMFS_H
#define RAMFS_H

#include <sys/types.h>
#include <sys/stat.h>

//Initializes in-memory filesystem
void ramfs_init(void);


//Locks the filesystem. Do this before operating on it on your thread.
void ramfs_lock(void);

//Unlocks the filesystem. Do this after operating on it on your thread.
void ramfs_unlock(void);


//Makes a new inode.
//Returns 0 on success or a negative error number.
int ramfs_make(ino_t dir, const char *name, mode_t mode, dev_t special, ino_t *ino_out);

//Finds an existing inode.
//Returns 0 on success or a negative error number.
int ramfs_find(ino_t dir, const char *name, ino_t *ino_out);

//Removes a directory entry, optionally from a specific inode.
int ramfs_unlink(ino_t dir, const char *name, ino_t rmino, int flags);

//Reads from an inode. Returns the number of bytes read or a negative error number.
ssize_t ramfs_read(ino_t ino, off_t off, void *buf, ssize_t len);

//Writes into an inode. Returns the number of bytes written or a negative error number.
ssize_t ramfs_write(ino_t ino, off_t off, const void *buf, ssize_t len);

//Truncates the given file to the given length.
int ramfs_trunc(ino_t ino, off_t size);

//Returns status information about the given file.
int ramfs_stat(ino_t ino, struct stat *st);

//Increments the reference-count of open files on the given inode.
void ramfs_inc(ino_t ino);

//Decrements the reference-count of open files on the given inode. May free it.
void ramfs_dec(ino_t ino);


#endif //RAMFS_H
