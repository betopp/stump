//systar.c
//Routine for unpacking system image into RAMfs
//Bryan E. Topp <betopp@betopp.com> 2021

#include "systar.h"
#include "kpage.h"
#include "kassert.h"
#include "file.h"
#include "m_kspc.h"
#include "m_frame.h"

#include <sys/stat.h>
#include <stdint.h>
#include <string.h>

//USTAR header block
typedef struct systar_ustar_hdr_s
{
	char filename[100];
	char mode_oct[8];
	char uid_oct[8];
	char gid_oct[8];
	char size_oct[12];
	char mtime_oct[12];
	char checksum_oct[8];
	char type;
	char linked[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devmaj[8];
	char devmin[8];
	char prefix[155];
	
} systar_ustar_hdr_t;

//Parses an octal string from a USTAR header
uint64_t systar_ustar_octstr(const char *str, size_t len)
{	
	uint64_t val = 0;
	for(size_t ss = 0; ss < len; ss++)
	{
		KASSERT(str[ss] == ' ' || str[ss] == '\0' || (str[ss] >= '0' && str[ss] <= '7'));
		if(str[ss] >= '0' && str[ss] <= '7')
		{
			val *= 8;
			val += str[ss] - '0';
		}
	}
	return val;
}

//Symbols defined by linker, giving bounds of system TAR linked into kernel
extern uint8_t _SYSTAR_START[];
extern uint8_t _SYSTAR_END[];

void systar_unpack(void)
{
	//System image as linked should be page-aligned so we can free it
	size_t systar_size = (size_t)(_SYSTAR_END - _SYSTAR_START);
	KASSERT((uintptr_t)_SYSTAR_END % m_frame_size() == 0);
	KASSERT((uintptr_t)_SYSTAR_START % m_frame_size() == 0);
	
	//Work through TAR one block at a time
	KASSERT(systar_size % 512 == 0);
	const uint8_t *block_bytes = _SYSTAR_START;
	int zero_blocks = 0;
	while(block_bytes < _SYSTAR_END)
	{
		//Read block header and advance past
		const systar_ustar_hdr_t *hdr = (systar_ustar_hdr_t*)(block_bytes);
		block_bytes += 512;
		
		//Make sure it's a USTAR header or empty - two empty blocks mean end-of-file
		if(hdr->filename[0] == '\0')
		{
			zero_blocks++;
			if(zero_blocks == 2)
				break;
			else
				continue;
		}
		
		KASSERT(memcmp(hdr->magic, "ustar\0", 6) == 0);
		KASSERT(memcmp(hdr->version, "00", 2) == 0);
		ssize_t file_size = systar_ustar_octstr(hdr->size_oct, sizeof(hdr->size_oct));
		
		//Based on the header, figure out the mode of the file we'll make
		mode_t mode = systar_ustar_octstr(hdr->mode_oct, sizeof(hdr->mode_oct)) & 0777;
		switch(hdr->type)
		{
			case '0':
			case '\0':
				mode |= S_IFREG;
				break;
			case '2':
				mode |= S_IFLNK;
				break;
			case '3':
				mode |= S_IFCHR;
				break;
			case '4':
				mode |= S_IFBLK;
				break;
			case '5':
				mode |= S_IFDIR;
				break;
			default:
				KASSERT(0);
		}
		
		//Read device numbers, if specified.
		uint64_t spec = systar_ustar_octstr(hdr->devmaj, sizeof(hdr->devmaj)) & 0xFFFF;
		spec <<= 16;
		spec |= systar_ustar_octstr(hdr->devmin, sizeof(hdr->devmin)) & 0xFFFF;
		
		//Make and open the file...
		const char *path_remain = hdr->filename;
		
		//Skip leading slashes and dots in the path. We're extracting to the root anyway.
		while( (*path_remain == '/') || (*path_remain == '.') )
		{
			path_remain++;
		}
		
		//Parse each non-final pathname component. Make sure the directories exist. Change into them, starting from root.
		file_t *dir_file;
		int dir_file_result = file_find(NULL, "/", &dir_file);
		KASSERT(dir_file_result >= 0);
		dir_file->access = 7;
		while(1)
		{
			const char *slash = strchr(path_remain, '/');
			if(slash == NULL)
			{
				//No further slashes in the pathname.
				break;
			}
			
			//There's a slash. We need to look up the pathname component before it. Copy it out by itself.
			char dirname[100] = {0};
			for(size_t dd = 0; dd < sizeof(dirname) - 1; dd++)
			{
				if(path_remain[dd] == '/')
					break;
				
				dirname[dd] = path_remain[dd];
			}
			
			//Try to look up that directory
			file_t *next_dir_file;
			int next_dir_file_result = file_find(dir_file, dirname, &next_dir_file);
			
			//Make it, if it doesn't exist
			if(next_dir_file_result < 0)
			{
				next_dir_file_result = file_make(dir_file, dirname, S_IFDIR | 0755, 0, &next_dir_file);
			}
			
			KASSERT(next_dir_file_result >= 0);
			next_dir_file->access = 7;
			
			//Close the previous one and advance
			dir_file->refs = 0;
			file_unlock(dir_file);
			dir_file = next_dir_file;
			path_remain = slash + 1;
		}
		
		//Alright, we're in the directory we want. Make the file there.
		KASSERT(strchr(path_remain, '/') == NULL);
		
		//Directories in TAR files can end with a "/", in which case, we're already done.
		if(strlen(path_remain) > 0)
		{
			file_t *file;
			int file_result = file_make(dir_file, path_remain, mode, spec, &file);
			KASSERT(file_result >= 0);
			file->access = 7;
			
			//Write the contents into the file
			if(S_ISREG(mode))
			{
				ssize_t written = file_write(file, block_bytes, file_size);
				KASSERT(written == file_size);
			}
			else if(S_ISLNK(mode))
			{
				ssize_t written = file_write(file, hdr->linked, strlen(hdr->linked));
				KASSERT(written == (ssize_t)strlen(hdr->linked));
			}
			
			//Close the file
			file->refs = 0;
			file_unlock(file);
		}
		
		//Close the directory where we made the file
		dir_file->refs = 0;
		file_unlock(dir_file);
		
		//Advance past the file contents in the TAR
		size_t file_blocks = (file_size + 511) / 512;
		block_bytes += file_blocks * 512;
	}
	
	//Unmap and free the file as-loaded, now that the contents are in our in-memory filesystem.
	uintptr_t vaddr_start = (uintptr_t)_SYSTAR_START;
	uintptr_t vaddr_end = (uintptr_t)_SYSTAR_END;
	size_t pagesize = m_frame_size();
	for(uintptr_t vaddr = vaddr_start; vaddr < vaddr_end; vaddr += pagesize)
	{
		uintptr_t paddr = m_kspc_get(vaddr);
		KASSERT(paddr != 0);
		m_kspc_set(vaddr, 0);
		m_frame_free(paddr);
	}
}
