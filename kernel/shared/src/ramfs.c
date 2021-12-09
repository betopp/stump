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
	mode_t mode; //Type and permissions of file
	off_t size; //Total size in bytes
	int64_t nfiles; //Number of open files (file_t) that refer to this inode
	int64_t nlinks; //Number of directory entries in the filesystem that refer to this inode
	
} ramfs_ino_t;

//Block in our RAM filesystem - unit of allocation
#define RAMFS_BLOCK_SIZE 4096
typedef union ramfs_block_u
{
	ramfs_ino_t ino;
	int64_t nextfree;
	uint8_t data[RAMFS_BLOCK_SIZE];
	
} ramfs_block_t;

//Space pre-allocated from kernel for storing the RAM filesystem
#define RAMFS_BLOCK_MAX ((64*1024*1024) / RAMFS_BLOCK_SIZE)
static ramfs_block_t *ramfs_blocks;

//Free-list of FS blocks
static int64_t ramfs_freehead;

//Allocates a block in the filesystem. Returns its block number.
int64_t ramfs_alloc(void)
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
void ramfs_free(int64_t blknum)
{
	KASSERT(blknum != 0);
	ramfs_blocks[blknum].nextfree = ramfs_freehead;
	ramfs_freehead = blknum;
}

void ramfs_init(void)
{
	//Make sure we didn't botch the block size definition...
	KASSERT(sizeof(ramfs_block_t) == RAMFS_BLOCK_SIZE);
	
	ramfs_blocks = kpage_alloc(RAMFS_BLOCK_MAX * RAMFS_BLOCK_SIZE);
	if(ramfs_blocks == NULL)
		m_panic("ramfs_init no memory");
	
	//Build the initial free-list of all blocks except 0.
	for(int64_t bb = 1; bb < RAMFS_BLOCK_MAX; bb++)
	{
		ramfs_free(bb);
	}
	
	//Make the inode for the root directory in block 0
	memset(&(ramfs_blocks[0]), 0, sizeof(ramfs_blocks[0]));
	ramfs_blocks[0].ino.mode = S_IFDIR | 0777;	
}
