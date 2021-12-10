//ramfs.c
//In-memory filesystem
//Bryan E. Topp <betopp@betopp.com> 2021

#include "ramfs.h"
#include "kpage.h"
#include "kassert.h"
#include "m_panic.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

//Inode in our RAM filesystem
typedef struct ramfs_ino_s
{
	//Information about the file
	mode_t mode; //Type and permissions of file
	dev_t special; //Device number if this is a device special
	off_t size; //Total size in bytes
	
	//Reference counts
	int64_t nfiles; //Number of open files (file_t) that refer to this inode
	int64_t nlinks; //Number of directory entries in the filesystem that refer to this inode
	
	//Blocks holding pointers to data blocks
	#define RAMFS_DTABLE_MAX 1000
	int dtables[RAMFS_DTABLE_MAX];
	
} ramfs_ino_t;

//Reference to data blocks in RAM filesystem
typedef struct ramfs_dtable_s
{
	//Blocks holding data
	#define RAMFS_DBLOCK_MAX 1024
	int blocks[RAMFS_DBLOCK_MAX];
	
} ramfs_dtable_t;

//Block in our RAM filesystem - unit of allocation
#define RAMFS_BLOCK_SIZE 4096
typedef union ramfs_block_u
{
	ramfs_ino_t ino;
	ramfs_dtable_t dtable;
	int nextfree;
	uint8_t data[RAMFS_BLOCK_SIZE];
	
} ramfs_block_t;

//Space pre-allocated from kernel for storing the RAM filesystem
#define RAMFS_BLOCK_MAX ((64*1024*1024) / RAMFS_BLOCK_SIZE)
static ramfs_block_t *ramfs_blocks;

//Free-list of FS blocks
static int ramfs_freehead;

//Directory entry as stored in filesystem
typedef struct ramfs_dirent_s
{
	ino_t ino;
	char name[120];
} ramfs_dirent_t;

//Allocates a block in the filesystem. Returns its block number.
static int ramfs_alloc(void)
{
	if(ramfs_freehead == 0)
		return -ENOSPC;
	
	int64_t retval = ramfs_freehead;
	ramfs_freehead = ramfs_blocks[ramfs_freehead].nextfree;
	
	KASSERT(retval > 0 && retval < RAMFS_BLOCK_MAX);
	memset(&(ramfs_blocks[retval]), 0, sizeof(ramfs_blocks[retval]));
	return retval;
}

//Frees a block in the filesystem, putting it back on the free-list.
static void ramfs_free(int blknum)
{
	KASSERT(blknum != 0);
	ramfs_blocks[blknum].nextfree = ramfs_freehead;
	ramfs_freehead = blknum;
}

//Returns the block number of the data-block for the given offset in the given inode.
//Optionally tries to allocate blocks to back the given location.
static int ramfs_getblock(ino_t ino, off_t off, int alloc)
{
	if(off < 0)
		return -EINVAL;
	
	ramfs_ino_t *iptr = &(ramfs_blocks[ino].ino);
	
	off_t block_num = off / RAMFS_BLOCK_SIZE;
	
	off_t block_idx = block_num % RAMFS_DBLOCK_MAX;
	off_t table_idx = block_num / RAMFS_DBLOCK_MAX;
	if(table_idx >= RAMFS_DTABLE_MAX)
		return -EFBIG;
	
	if(iptr->dtables[table_idx] == 0)
	{
		if(!alloc)
			return 0;
		
		int newtable = ramfs_alloc();
		if(newtable < 0)
			return newtable;
		
		iptr->dtables[table_idx] = newtable;
	}
	
	ramfs_dtable_t *dptr = &(ramfs_blocks[iptr->dtables[table_idx]].dtable);
	if(dptr->blocks[block_idx] == 0)
	{
		if(!alloc)
			return 0;
		
		int newblock = ramfs_alloc();
		if(newblock < 0)
			return newblock;
		
		dptr->blocks[block_idx] = newblock;
	}
	
	return dptr->blocks[block_idx];
}

void ramfs_init(void)
{
	//Make sure we didn't botch the block size definition...
	KASSERT(sizeof(ramfs_block_t) == RAMFS_BLOCK_SIZE);
	
	ramfs_blocks = kpage_alloc(RAMFS_BLOCK_MAX * RAMFS_BLOCK_SIZE);
	if(ramfs_blocks == NULL)
		m_panic("ramfs_init no memory");
	
	//Build the initial free-list of all blocks except 0.
	for(int bb = 1; bb < RAMFS_BLOCK_MAX; bb++)
	{
		ramfs_free(bb);
	}
	
	//Make the inode for the root directory in block 0
	memset(&(ramfs_blocks[0]), 0, sizeof(ramfs_blocks[0]));
	ramfs_blocks[0].ino.mode = S_IFDIR | 0777;	
	
	//Write "." and ".." into root directory
	ramfs_dirent_t dot = { .ino = 0, .name = "." };
	ssize_t dot_written = ramfs_write(0, 0, &dot, sizeof(dot));
	if(dot_written != sizeof(dot))
		m_panic("ramfs_init failed making root dir");
	
	ramfs_dirent_t dotdot = { .ino = 0, .name = ".." };
	ssize_t dotdot_written = ramfs_write(0, sizeof(dot), &dotdot, sizeof(dotdot));
	if(dotdot_written != sizeof(dotdot))
		m_panic("ramfs_init failed making root dir");
}

int ramfs_make(ino_t dir, const char *name, mode_t mode, dev_t special, ino_t *ino_out)
{
	if(name == NULL || name[0] == '\0')
		return -EINVAL;
	
	//Error to return on failure
	int err_ret = 0;
	
	//Allocate a block for the new inode.
	int inoblock = ramfs_alloc();
	if(inoblock < 0)
		return inoblock;
	
	ramfs_ino_t *newino = &(ramfs_blocks[inoblock].ino);
	newino->mode = mode;
	newino->special = special;
	newino->size = 0;
	
	//Try to find the name in the directory. It shouldn't exist.
	ino_t find_ino = 0;
	int find_err = ramfs_find(dir, name, &find_ino);
	if(find_err >= 0)
	{
		//Found an existing file with this name in this directory.
		err_ret = -EEXIST;
		goto failure;
	}
	else if(find_err != -ENOENT)
	{
		//Some other error occurred while trying to search the directory.
		err_ret = find_err;
		goto failure;
	}
	
	//If we're making a directory, write the "." and ".." entries.
	if(S_ISDIR(mode))
	{
		ramfs_dirent_t dot = { .ino = inoblock, .name = "." };
		ssize_t dot_written = ramfs_write(inoblock, 0, &dot, sizeof(dot));
		if(dot_written != sizeof(dot))
		{
			err_ret = (dot_written < 0) ? dot_written : -EIO;
			goto failure;
		}
		
		ramfs_dirent_t dotdot = { .ino = dir, .name = ".." };
		ssize_t dotdot_written = ramfs_write(inoblock, sizeof(dot), &dotdot, sizeof(dotdot));
		if(dotdot_written != sizeof(dotdot))
		{
			err_ret = (dotdot_written < 0) ? dotdot_written : -EIO;
			goto failure;
		}
	}
	
	//Make the directory entry that points to the new file, with its initial name.
	ramfs_dirent_t firstlink = { .ino = inoblock };
	if(strlen(name) >= sizeof(firstlink.name))
	{
		err_ret = -ENAMETOOLONG;
		goto failure;
	}
	strncpy(firstlink.name, name, sizeof(firstlink.name));
	firstlink.name[sizeof(firstlink.name)-1] = '\0';
	
	//Write into the directory containing the file.
	//Do this last so we don't need to un-do it on failure.
	ramfs_ino_t *dirino = &(ramfs_blocks[dir].ino);
	ssize_t firstlink_written = ramfs_write(dir, dirino->size, &firstlink, sizeof(firstlink));
	if(firstlink_written != sizeof(firstlink))
	{
		err_ret = (firstlink_written < 0) ? firstlink_written : -EIO;
		goto failure;
	}
	
	//The new file starts with one link in the filesystem and one open file.
	newino->nfiles = 1;
	newino->nlinks = 1;
	*ino_out = inoblock;
	return 0;
	
failure:
	if(inoblock > 0)
	{
		ramfs_trunc(inoblock, 0);
		ramfs_free(inoblock);
	}
	KASSERT(err_ret < 0);
	*ino_out = ~0ul;
	return err_ret;
}

int ramfs_find(ino_t dir, const char *name, ino_t *ino_out)
{	
	KASSERT(dir < RAMFS_BLOCK_MAX);
	
	if(name == NULL || name[0] == '\0')
	{
		//Looking up "" - return the given file.
		*ino_out = dir;
		ramfs_blocks[dir].ino.nfiles++;
		return 0;
	}
	
	ramfs_ino_t *dptr = &(ramfs_blocks[dir].ino);
	if(!S_ISDIR(dptr->mode))
		return -ENOTDIR;
	
	//Read all directory entries and look for this name.
	off_t nextoff = 0;
	while(nextoff < dptr->size)
	{
		ramfs_dirent_t de;
		ssize_t read = ramfs_read(dir, nextoff, &de, sizeof(de));
		if(read < 0)
			return read;
		if(read != sizeof(de))
			return -EIO;
		
		if(!strcmp(de.name, name))
		{
			//Found it - output the inode and give it another file reference.
			*ino_out = de.ino;
			ramfs_blocks[de.ino].ino.nfiles++;
			return 0;
		}
		
		//Not the file we were looking for.
		nextoff += read;
	}
	
	//Didn't find an entry with this name
	return -ENOENT;
}

ssize_t ramfs_read(ino_t ino, off_t off, void *buf, ssize_t len)
{
	if(off < 0)
		return -EINVAL;
	
	KASSERT(ino < RAMFS_BLOCK_MAX);
	ramfs_ino_t *iptr = &(ramfs_blocks[ino].ino);
	
	//Handle a block at a time
	uint8_t *buf_bytes = (uint8_t*)buf;
	ssize_t completed = 0;
	while(len > 0 && off < iptr->size)
	{
		//Cannot read past the end of the block, nor the end of the file in bytes, nor the requested length.
		ssize_t left_in_block = RAMFS_BLOCK_SIZE - (off % RAMFS_BLOCK_SIZE);
		ssize_t left_in_file = iptr->size - off;
		ssize_t read_len = len;
		if(read_len > left_in_block)
			read_len = left_in_block;
		if(read_len > left_in_file)
			read_len = left_in_file;
		
		//Find where the data for this block is - or return 0s if it's a hole (no data yet).
		int dataloc = ramfs_getblock(ino, off, 0);
		if(dataloc < 0)
			return (completed > 0) ? completed : dataloc;
		
		if(dataloc == 0)
		{
			//No data here - read zeroes.
			memset(buf_bytes, 0, read_len);
		}
		else
		{
			//Data here - copy it out
			memcpy(buf_bytes, ramfs_blocks[dataloc].data + (off % RAMFS_BLOCK_SIZE), read_len);
		}
		
		completed += read_len;
		buf_bytes += read_len;
		off += read_len;
		len -= read_len;
	}
	
	KASSERT(len >= 0);
	return completed;
}

ssize_t ramfs_write(ino_t ino, off_t off, const void *buf, ssize_t len)
{
	if(off < 0)
		return -EINVAL;
	
	KASSERT(ino < RAMFS_BLOCK_MAX);
	ramfs_ino_t *iptr = &(ramfs_blocks[ino].ino);
	
	//Handle a block at a time
	const uint8_t *buf_bytes = (uint8_t*)buf;
	ssize_t completed = 0;
	while(len > 0)
	{
		ssize_t left_in_block = RAMFS_BLOCK_SIZE - (off % RAMFS_BLOCK_SIZE);
		ssize_t write_len = len;
		if(write_len > left_in_block)
			write_len = left_in_block;
		
		//Allocate new data block if there's none here yet
		int dataloc = ramfs_getblock(ino, off, 1);
		if(dataloc < 0)
			return (completed > 0) ? completed : dataloc;
		
		KASSERT(dataloc > 0);

		//Copy the data in
		memcpy(ramfs_blocks[dataloc].data + (off % RAMFS_BLOCK_SIZE), buf_bytes, write_len);
		
		completed += write_len;
		buf_bytes += write_len;
		off += write_len;
		len -= write_len;
		
		//Expand filesize if we've written past its old end
		if(off > iptr->size)
			iptr->size = off;
	}
	
	KASSERT(len >= 0);
	return completed;	
}

int ramfs_trunc(ino_t ino, off_t size)
{	
	if(size < 0)
		return -EINVAL;
	
	if(size > (off_t)(1ul * RAMFS_DTABLE_MAX * RAMFS_DBLOCK_MAX * RAMFS_BLOCK_SIZE))
		return -EFBIG;
	
	//Change size of file
	KASSERT(ino < RAMFS_BLOCK_MAX);
	ramfs_ino_t *iptr = &(ramfs_blocks[ino].ino);
	iptr->size = size;
	
	//Ditch any tables that are totally off the end of the file
	const size_t table_size = RAMFS_BLOCK_SIZE * RAMFS_DBLOCK_MAX;
	size_t tables_needed = (size + table_size - 1) / table_size;
	for(size_t tt = tables_needed; tt < RAMFS_DTABLE_MAX; tt++)
	{
		if(iptr->dtables[tt] == 0)
			continue;
		
		ramfs_dtable_t *dptr = &(ramfs_blocks[iptr->dtables[tt]].dtable);
		for(size_t bb = 0; bb < RAMFS_DBLOCK_MAX; bb++)
		{
			if(dptr->blocks[bb] == 0)
				continue;
			
			ramfs_free(dptr->blocks[bb]);
			dptr->blocks[bb] = 0;
		}
		
		ramfs_free(iptr->dtables[tt]);
		iptr->dtables[tt] = 0;
	}
	
	//Ditch unused blocks from any partially-used table at the end of the file
	size_t blocks_needed = (size + RAMFS_BLOCK_SIZE - 1) / RAMFS_BLOCK_SIZE;
	if((tables_needed > 0) && (blocks_needed % RAMFS_DBLOCK_MAX))
	{
		size_t partial_table = tables_needed - 1;
		size_t first_block = partial_table * RAMFS_DBLOCK_MAX;
		if(iptr->dtables[partial_table] != 0)
		{
			ramfs_dtable_t *dptr = &(ramfs_blocks[iptr->dtables[partial_table]].dtable);
			for(size_t bb = blocks_needed - first_block; bb < RAMFS_DBLOCK_MAX; bb++)
			{
				if(dptr->blocks[bb] == 0)
					continue;
				
				ramfs_free(dptr->blocks[bb]);
				dptr->blocks[bb] = 0;
			}
		}
	}
	
	//If we truncated to zero, we should have no blocks referenced from this inode.
	if(size == 0)
	{
		for(int tt = 0; tt < RAMFS_DTABLE_MAX; tt++)
		{
			KASSERT(iptr->dtables[tt] == 0);
		}
	}
	
	return 0;
}
