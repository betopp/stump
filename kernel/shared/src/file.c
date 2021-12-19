//file.c
//Table of open files in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "file.h"
#include "ramfs.h"
#include "m_spl.h"
#include "kassert.h"
#include "pipe.h"
#include "sc.h"
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "d_log.h"
#include "d_null.h"
#include "d_nxio.h"
#include "d_con.h"

//All files currently open on the system.
#define FILE_MAX 1024
static file_t file_table[FILE_MAX];

//Character devices supported
typedef enum file_chrdev_major_e
{
	FILE_CHRDEV_MAJOR_NULL = 0,
	FILE_CHRDEV_MAJOR_CON  = 1,
	FILE_CHRDEV_MAJOR_LOG  = 2,
	FILE_CHRDEV_MAJOR_NXIO = 3,
	
	FILE_CHRDEV_MAJOR_MAX
} file_chrdev_major_t;

//Character device switch
typedef struct file_chrdev_s
{
	int     (*open) (int minor);
	void    (*close)(int minor);
	ssize_t (*read) (int minor, void *buf, ssize_t nbytes);
	ssize_t (*write)(int minor, const void *buf, ssize_t nbytes);
	int     (*ioctl)(int minor, int operation, void *buf, ssize_t len);
} file_chrdev_t;
static const file_chrdev_t file_chrdev_table[FILE_CHRDEV_MAJOR_MAX] = 
{
	[FILE_CHRDEV_MAJOR_NULL] =
	{
		.read = d_null_read,
		.write = d_null_write,
	},
	[FILE_CHRDEV_MAJOR_CON] =
	{
		.open = d_con_open,
		.close = d_con_close,
		.ioctl = d_con_ioctl,
	},
	[FILE_CHRDEV_MAJOR_LOG] =
	{
		.write = d_log_write,
	},
	[FILE_CHRDEV_MAJOR_NXIO] = 
	{
		.open = d_nxio_open,
		.close = d_nxio_close,
		.read = d_nxio_read,
		.write = d_nxio_write,
		.ioctl = d_nxio_ioctl,
	},
};

//Returns character-device functions for the given character-device number.
//Returns functions that return -NXIO if invalid value is given.
static const file_chrdev_t *file_chrdev_major(int special)
{
	int major = (special >> 16) & 0xFFFF;
	if(major < 0 || major >= FILE_CHRDEV_MAJOR_MAX)
		return &(file_chrdev_table[FILE_CHRDEV_MAJOR_NXIO]);
	
	return &(file_chrdev_table[major]);
}

//Returns the minor number associated with the given character-device number.
static int file_chrdev_minor(int special)
{
	return special & 0xFFFF;
}


//Gets a free entry from the file table. Returns it with the lock held.
static file_t *file_lockfree(void)
{
	for(int ff = 0; ff < FILE_MAX; ff++)
	{
		//Don't bother fighting over locks - a file entry already locked is already in use.
		file_t *fptr = &(file_table[ff]);
		if(m_spl_try(&(fptr->spl)))
		{
			if(fptr->refs == 0)
			{
				//Got an unused file entry locked.
				return fptr;
			}
			
			//File entry already used - let go of lock and keep looking
			m_spl_rel(&(fptr->spl));
		}
	}	
	
	//Didn't find any free entries.
	return NULL;
}

int file_make(file_t *dir, const char *name, mode_t mode, dev_t special, file_t **file_out)
{
	if(dir == NULL)
		return -EBADF;
	
	if(!(dir->access & _SC_ACCESS_W))
		return -EBADF;
	
	if(!S_ISDIR(dir->mode))
		return -ENOTDIR;
	
	if(name == NULL || name[0] == '\0')
		return -EINVAL;
	
	if((!strcmp(name, ".")) || (!strcmp(name, "..")))
		return -EINVAL;
	
	if(!(S_ISDIR(mode) || S_ISCHR(mode) || S_ISREG(mode) || S_ISLNK(mode) || S_ISFIFO(mode)))
		return -EINVAL;
	
	//Make room for the new open file entry.
	file_t *newfile = file_lockfree();
	if(newfile == NULL)
		return -ENFILE;
	
	//If this is a named pipe, it needs a pipe structure.
	if(S_ISFIFO(mode))
	{
		pipe_t *pipe = NULL;
		int pipe_err = pipe_new(&pipe);
		if(pipe_err < 0)
		{
			//Failed to make pipe
			file_unlock(newfile);
			return pipe_err;
		}
		special = pipe->id;
		pipe->refs_file++;
		pipe_unlock(pipe);
	}
		
	//Try to make the inode in the filesystem
	ramfs_lock();
	ino_t ino_made = 0;
	int make_err = ramfs_make(dir->ino, name, mode, special, &ino_made);
	if(make_err < 0)
	{
		if(S_ISFIFO(mode))
		{
			pipe_t *pipe = pipe_lockid(special);
			KASSERT(pipe != NULL);
			pipe->refs_file = 0;
			pipe_unlock(pipe);
		}
		
		ramfs_unlock();
		m_spl_rel(&(newfile->spl));
		return make_err;
	}
	
	//Made the inode - store reference to it.
	newfile->ino = ino_made;
	ramfs_inc(ino_made);
	
	//Done with RAM FS at this point
	ramfs_unlock();
	
	//If we're making a device node, make sure the device is OK with that. Close the file otherwise.
	if(S_ISCHR(mode))
	{
		const file_chrdev_t *major = file_chrdev_major(special);
		if(major->open != NULL)
		{
			int dev_err = (*(major->open))(file_chrdev_minor(special));
			if(dev_err < 0)
			{
				//Device said "no"
				ramfs_lock();
				ramfs_dec(ino_made);
				ramfs_unlock();
				
				newfile->ino = 0;
				m_spl_rel(&(newfile->spl));
				return dev_err;
			}
		}
	}
	
	newfile->mode = mode;
	newfile->special = special;
	newfile->off = 0;
	newfile->access = 0;
	newfile->refs = 1;
	
	*file_out = newfile;
	return 0;
}

int file_find(file_t *dir, const char *name, file_t **file_out)
{
	//Make sure we've got room for the new file.
	file_t *newfile = file_lockfree();
	if(newfile == NULL)
	{
		//No room in file table.
		return -ENFILE;
	}
	
	//See what we're trying to find...
	ramfs_lock(); //Lock before lookup so files don't disappear underneath us
	ino_t found_ino = 0;
	if(name != NULL && name[0] == '/' && name[1] == '\0')
	{
		//Looking for "/". Directory doesn't matter. Always find inode 0.
		found_ino = 0;
	}
	else
	{
		//Other cases require a valid starting point to refer to.
		if(dir == NULL)
		{
			ramfs_unlock();
			m_spl_rel(&(newfile->spl));
			return -EINVAL;
		}
		
		if(name == NULL || name[0] == '\0')
		{
			//Looking for empty-string. Return another reference to the given file, even if it's not a dir.
			found_ino = dir->ino;
		}
		else
		{
			//Actually doing a directory lookup.
			int find_err = ramfs_find(dir->ino, name, &found_ino);			
			if(find_err < 0)
			{
				ramfs_unlock();
				m_spl_rel(&(newfile->spl));
				return find_err;
			}
		}
	}
		
	//Alright, found the file. Add a reference and get its info.
	newfile->ino = found_ino;
	ramfs_inc(found_ino);
	
	struct stat st;
	int stat_err = ramfs_stat(found_ino, &st);
	KASSERT(stat_err == 0);
	
	//Done with filesystem at this point - we hold reference to the file, it won't go away.
	ramfs_unlock();
	
	//If we're opening a device, make sure the device is OK with that. Close the file otherwise.
	if(S_ISCHR(st.st_mode))
	{
		const file_chrdev_t *major = file_chrdev_major(st.st_rdev);
		if(major->open != NULL)
		{
			int dev_err = (*(major->open))(file_chrdev_minor(st.st_rdev));
			if(dev_err < 0)
			{
				//Device said "no"
				ramfs_lock();
				ramfs_dec(found_ino);
				ramfs_unlock();
				
				newfile->ino = 0;
				m_spl_rel(&(newfile->spl));
				return dev_err;
			}
		}
	}
	
	newfile->mode = st.st_mode;
	newfile->special = st.st_rdev;
	newfile->off = 0;
	newfile->access = 0;
	newfile->refs = 1;
	
	//Return the new file with one reference, still locked.
	*file_out = newfile;
	return 0;
}

int file_unlink(file_t *dir, const char *name, file_t *rmfile, int flags)
{
	if(dir == NULL)
		return -EBADF;
	
	if(!(dir->access & _SC_ACCESS_W))
		return -EBADF;
	
	if(!S_ISDIR(dir->mode))
		return -ENOTDIR;
	
	ramfs_lock();
	int result = ramfs_unlink(dir->ino, name, (rmfile != NULL) ? rmfile->ino : 0, flags);
	ramfs_unlock();
	
	return result;
}

ssize_t file_read(file_t *file, void *buf, ssize_t nbytes)
{
	if(!(file->access & _SC_ACCESS_R))
		return -EBADF;
	
	if(S_ISCHR(file->mode))
	{
		const file_chrdev_t *major = file_chrdev_major(file->special);
		if(major->read == NULL)
			return -ENOTTY;
		
		return (*(major->read))(file_chrdev_minor(file->special), buf, nbytes);
	}
	
	ramfs_lock();
	ssize_t retval = ramfs_read(file->ino, file->off, buf, nbytes);
	ramfs_unlock();
	
	if(retval < 0)
		return retval;
	
	file->off += retval;
	return retval;
}

ssize_t file_write(file_t *file, const void *buf, ssize_t nbytes)
{
	if(!(file->access & _SC_ACCESS_W))
		return -EBADF;
	
	if(S_ISCHR(file->mode))
	{
		const file_chrdev_t *major = file_chrdev_major(file->special);
		if(major->write == NULL)
			return -ENOTTY;
		
		return (*(major->write))(file_chrdev_minor(file->special), buf, nbytes);
	}
	
	if(S_ISDIR(file->mode))
		return -EISDIR;
	
	ramfs_lock();
	ssize_t retval = ramfs_write(file->ino, file->off, buf, nbytes);
	ramfs_unlock();
	
	if(retval < 0)
		return retval;
	
	file->off += retval;
	return retval;
}

int file_trunc(file_t *file, off_t size)
{
	if(!(file->access & _SC_ACCESS_W))
		return -EBADF;
	
	if(S_ISDIR(file->mode))
		return -EISDIR;
	
	if(S_ISCHR(file->mode))
		return -ENOTTY;
	
	ramfs_lock();
	int retval = ramfs_trunc(file->ino, size);
	ramfs_unlock();
	
	return retval;
}

int file_stat(file_t *file, struct stat *st)
{
	ramfs_lock();
	int retval = ramfs_stat(file->ino, st);
	ramfs_unlock();
	return retval;
}

off_t file_seek(file_t *file, off_t offset, int whence)
{
	off_t whence_val = 0;
	switch(whence)
	{
		case SEEK_SET:
		{
			whence_val = 0;
			break;
		}
		case SEEK_CUR:
		{
			whence_val = file->off;
			break;
		}
		case SEEK_END:
		{
			struct stat st;
			ramfs_lock();
			int stat_err = ramfs_stat(file->ino, &st);
			ramfs_unlock();
			
			if(stat_err < 0)
				return stat_err;
			
			whence_val = st.st_size;
			break;
		}
		default:
			return -EINVAL;
	}
	
	file->off = whence_val + offset;
	if(file->off < 0)
		file->off = 0;
	
	return file->off;
}

int file_ioctl(file_t *file, int operation, void *buf, ssize_t len)
{
	if(!S_ISCHR(file->mode))
		return -ENOTTY;
	
	const file_chrdev_t *major = file_chrdev_major(file->special);
	if(major->ioctl == NULL)
		return -ENOTTY;
	
	return (*(major->ioctl))(file_chrdev_minor(file->special), operation, buf, len);
}

void file_lock(file_t *file)
{
	m_spl_acq(&(file->spl));
}

void file_unlock(file_t *file)
{
	if(file->refs == 0)
	{
		ramfs_lock();
		ramfs_dec(file->ino);
		ramfs_unlock();
		
		if(S_ISCHR(file->mode))
		{
			const file_chrdev_t *major = file_chrdev_major(file->special);
			if(major->close != NULL)
				(*(major->close))(file_chrdev_minor(file->special));
		}
		
		if(S_ISFIFO(file->mode))
		{
			pipe_t *pipe = pipe_lockid(file->special);
			KASSERT(pipe != NULL);
			if(file->access & _SC_ACCESS_R)
				pipe->refs_file_r--;
			if(file->access & _SC_ACCESS_W)
				pipe->refs_file_w--;
			
			pipe->refs_file--;
			pipe_unlock(pipe);
		}
		
		file->ino = 0;
		file->off = 0;
		file->mode = 0;
		file->special = 0;
	}
	
	m_spl_rel(&(file->spl));
}



