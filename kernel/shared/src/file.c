//file.c
//Table of open files in kernel
//Bryan E. Topp <betopp@betopp.com> 2021

#include "file.h"
#include "ramfs.h"
#include "m_spl.h"
#include "kassert.h"
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>

//All files currently open on the system.
#define FILE_MAX 1024
static file_t file_table[FILE_MAX];

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
		return -EINVAL;
	
	if(!S_ISDIR(dir->mode))
		return -ENOTDIR;
	
	if(name == NULL || name[0] == '\0')
		return -EINVAL;
	
	if((!strcmp(name, ".")) || (!strcmp(name, "..")))
		return -EINVAL;
	
	if(!(S_ISDIR(mode) || S_ISCHR(mode) || S_ISREG(mode) || S_ISLNK(mode)))
		return -EINVAL;
	
	//Make room for the new open file entry.
	file_t *newfile = file_lockfree();
	if(newfile == NULL)
	{
		//No room in file table.
		return -ENFILE;
	}
	
	ramfs_lock();
	
	//Try to make the inode in the filesystem
	ino_t ino_made = 0;
	int make_err = ramfs_make(dir->ino, name, mode, special, &ino_made);
	if(make_err < 0)
	{
		ramfs_unlock();
		m_spl_rel(&(newfile->spl));
		return make_err;
	}
	
	//Made the inode - store reference to it.
	newfile->ino = ino_made;
	ramfs_inc(ino_made);
	
	//Done with RAM FS at this point
	ramfs_unlock();
	
	newfile->mode = mode;
	newfile->special = special;
	newfile->off = 0;
	
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
	
	newfile->mode = st.st_mode;
	newfile->special = st.st_rdev;
	newfile->off = 0;
	
	//Return the new file with one reference, still locked.
	newfile->refs = 1;
	*file_out = newfile;
	return 0;
}

ssize_t file_read(file_t *file, void *buf, ssize_t nbytes)
{
	if(S_ISCHR(file->mode))
		return -ENOSYS;
	
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
	if(S_ISCHR(file->mode))
		return -ENOSYS;
	
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
	if(S_ISDIR(file->mode))
		return -EISDIR;
	
	if(S_ISCHR(file->mode))
		return -ENOTTY;
	
	ramfs_lock();
	int retval = ramfs_trunc(file->ino, size);
	ramfs_unlock();
	
	return retval;
}

void file_unlock(file_t *file)
{
	if(file->refs == 0)
	{
		ramfs_lock();
		ramfs_dec(file->ino);
		ramfs_unlock();
		file->ino = 0;
		file->off = 0;
		file->mode = 0;
		file->special = 0;
	}
	
	m_spl_rel(&(file->spl));
}



