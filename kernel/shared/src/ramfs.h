//ramfs.h
//In-memory filesystem
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef RAMFS_H
#define RAMFS_H

#include <sys/types.h>

//Initializes in-memory filesystem
void ramfs_init(void);

//Makes a new inode. The inode starts with one open-file reference.
//Returns 0 on success or a negative error number.
int ramfs_make(ino_t dir, const char *name, mode_t mode, dev_t special, ino_t *ino_out);

//Finds an existing inode. The inode is given an additional open-file reference.
//Returns 0 on success or a negative error number.
int ramfs_find(ino_t dir, const char *name, ino_t *ino_out);

//Reads from an inode. Returns the number of bytes read or a negative error number.
ssize_t ramfs_read(ino_t ino, off_t off, void *buf, ssize_t len);

//Writes into an inode. Returns the number of bytes written or a negative error number.
ssize_t ramfs_write(ino_t ino, off_t off, const void *buf, ssize_t len);

//Truncates the given file to the given length.
int ramfs_trunc(ino_t ino, off_t size);


#endif //RAMFS_H
