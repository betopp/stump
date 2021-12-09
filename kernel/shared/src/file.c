//file.c
//Table of open files in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "file.h"
#include <errno.h>

//All files currently open on the system.
#define FILE_MAX 1024
static file_t file_table[FILE_MAX];


int file_find(file_t *dir, const char *name, file_t **file_out)
{
	(void)dir;
	(void)name;
	(void)file_out;
	(void)file_table;
	return -ENOSYS;
}

int file_make(file_t *dir, const char *name, mode_t mode, dev_t special, file_t **file_out)
{
	(void)dir;
	(void)name;
	(void)mode;
	(void)special;
	(void)file_out;
	(void)file_table;
	return -ENOSYS;
}

ssize_t file_write(file_t *file, const void *buf, ssize_t nbytes)
{
	(void)file;
	(void)buf;
	(void)nbytes;
	return -ENOSYS;
}

void file_unlock(file_t *file)
{
	if(file->refs == 0)
	{
		//Todo - tell RAM FS to drop reference to the inode
		file->ino = 0;
		file->off = 0;
		file->mode = 0;
		file->special = 0;
	}
	
	m_spl_rel(&(file->spl));
}