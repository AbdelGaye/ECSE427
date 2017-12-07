
#include "sfs_api.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <strings.h>
#include "disk_emu.h"
#define GAYE_ABDELKADER_DISK "sfs_disk.disk"
#define BLOCK_SIZE 1024
#define NUM_BLOCKS 1024  //maximum number of data blocks on the disk
#define NO_OF_INODES 100
#define NO_BLOCKS_ITB ((NO_OF_INODES)*(sizeof(inode_t))/BLOCK_SIZE + (((NO_OF_INODES)*(sizeof(inode_t)))%BLOCK_SIZE > 0))
#define NO_BLOCKS_DIR ((NO_OF_INODES-1)*(sizeof(directory_entry))/BLOCK_SIZE + (((NO_OF_INODES-1)*(sizeof(directory_entry)))%BLOCK_SIZE > 0))
#define STR_DATABLOCKS (1+NO_BLOCKS_ITB) // The start of the datablocks i.e. superblock (1) + number of blocks for the inode table
#define BITMAP_ROW_SIZE (NUM_BLOCKS/8) // this essentially mimcs the number of rows we have in the bitmap. we will have 128 rows. 

struct superblock_t superb;
struct inode_t in_table[NO_OF_INODES];
struct file_descriptor fd_table[NO_OF_INODES]; 
struct directory_entry rootDir[(NO_OF_INODES-1)];

void* buffer;
int num_files, file_track;


/* macros */
#define FREE_BIT(_data, _which_bit) \
    _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
    _data = _data & ~(1 << _which_bit)

//initialize all bits to high
uint8_t f_bit_map[BITMAP_ROW_SIZE] = { [0 ... BITMAP_ROW_SIZE - 1] = UINT8_MAX };


//Allocating a block for the superblock
int blockForSuperb()
{
	buffer = (void*) malloc(BLOCK_SIZE);
	memset(buffer, 0, BLOCK_SIZE);
	memcpy(buffer, &superb, sizeof(superblock_t));
	//Occupying the first block
	int num_blocks = write_blocks(0, 1, buffer);
	free(buffer);

	return num_blocks;
}

int readSuperb()
{
	//Reading the superblock
	buffer = (void*) malloc(BLOCK_SIZE);
	memset(buffer, 0, BLOCK_SIZE);
	int num_blocks = read_blocks(0, 1, buffer);
	memcpy(&superb, buffer, sizeof(superblock_t));
	free(buffer);

	return num_blocks;
}

//Allocating blocks for the inodes
int blocksForInodeT() 
{ 
	buffer = (void*) malloc(NO_BLOCKS_ITB*BLOCK_SIZE);
	memset(buffer, 0, NO_BLOCKS_ITB*BLOCK_SIZE);
	memcpy(buffer, in_table, (NO_BLOCKS_ITB)*(sizeof(inode_t)));
	int num_blocks = write_blocks(1, NO_BLOCKS_ITB, buffer);
	free(buffer);

	return num_blocks;
}

int readInode()
{
	//Reading the inode table
	buffer = (void*) malloc(NO_BLOCKS_ITB*BLOCK_SIZE);
	memset(buffer, 0, NO_BLOCKS_ITB*BLOCK_SIZE);
	int num_blocks = read_blocks(1, NO_BLOCKS_ITB, buffer);
	memcpy(in_table, buffer, NO_BLOCKS_ITB*sizeof(inode_t));
	free(buffer);

	return num_blocks;
}

//Allocating blocks for the directory
int blocksForDir() 
{ 
	buffer = (void*) malloc(NO_BLOCKS_DIR*BLOCK_SIZE);
	memset(buffer, 0, NO_BLOCKS_DIR*BLOCK_SIZE);
	memcpy(buffer, rootDir, (NO_OF_INODES-1)*(sizeof(directory_entry)));
	int num_blocks = write_blocks(STR_DATABLOCKS, NO_BLOCKS_DIR, buffer);
	free(buffer);

	return num_blocks;
}

//Allocating blocks for the bitmap table
int blockForBitmap() 
{ 
	buffer = (void*) malloc(BLOCK_SIZE);
	//Allocating free spaces in the bitmap
	memset(buffer, 1, BLOCK_SIZE);
	memcpy(buffer, f_bit_map, (BITMAP_ROW_SIZE)*(sizeof(uint8_t)));
	//Occupying the last block
	int num_blocks = write_blocks(NUM_BLOCKS-1, 1, buffer);
	free(buffer);

	return num_blocks;
}

int readBitmap()
{
	//Reading the bitmap table
	buffer = (void*) malloc(BLOCK_SIZE);
	memset(buffer, 1, BLOCK_SIZE);
	int num_blocks = read_blocks(NUM_BLOCKS-1, 1, buffer);
	memcpy(f_bit_map, buffer, (BITMAP_ROW_SIZE)*(sizeof(uint8_t)));
	free(buffer);

	return num_blocks;
}



//Initializing the file descriptor table
void init_fdt()
{
	int i;
	//Starting at index 1 because of the superblock
	for(i = 1; i < NO_OF_INODES; i++) 
	{
		fd_table[i].inodeIndex = -1;
		fd_table[i].inode = NULL; 
		fd_table[i].rwptr = 0;
	}
}

//Initializing the inode table 
void init_int()
{
	int i, j;
	//0777 is for the permission to read, write and execute
	//Starting with the entry for the superblock
	in_table[0].mode = 0777;
	in_table[0].link_cnt = 0;
	in_table[0].uid = 0;
	in_table[0].gid = 0;
	in_table[0].size = 0;

	for(i = 0; i < 12; i++)
	{
		if(i < NO_BLOCKS_DIR)
			in_table[0].data_ptrs[i] = STR_DATABLOCKS+i;
		else
			in_table[0].data_ptrs[i] = -1;
	}

	in_table[0].indirectPointer = -1;

	//Now for the inodes for files
	for(i = 1; i < NO_OF_INODES; i++)
	{
		in_table[i].mode = 0777;
		in_table[i].link_cnt = -1;
		in_table[i].uid = -1;
		in_table[i].gid = -1;
		in_table[i].size = -1;

		for(j = 0; j < 12; j++)
		{
			in_table[i].data_ptrs[j] = -1;
		}

		in_table[i].indirectPointer = -1;
	}
}

//Initializing the superblock
void init_superb()
{	
	superb.magic = 0xACBD005;
	superb.block_size = BLOCK_SIZE;
	superb.fs_size = NUM_BLOCKS*BLOCK_SIZE;
	superb.inode_table_len = NO_OF_INODES;
	superb.root_dir_inode = 0;	
}

void mksfs(int fresh) 
{
	//If the disk has not been initialized
	if(fresh)
	{
		init_fdt();

		init_int();

		init_superb();

		init_fresh_disk(GAYE_ABDELKADER_DISK, BLOCK_SIZE, NUM_BLOCKS);

		//Writing the superblock
		if(blockForSuperb() < 0)
			printf("Cannot write the superblock to disk.\n");
	
		//Reserving the first block for the superblock
		force_set_index(0);

		//Writing the inodes
		if(blocksForInodeT() < 0)
			printf("Cannot write the inodes to disk.\n");
	
		int i;
		//Occupying the inodes in the bitmap table
		for(i = 0; i < NO_BLOCKS_ITB; i++)
		{
			force_set_index(i+1);
		}

		//Initializing the directory entries
		for(i = 0; i < (NO_OF_INODES-1); i++) 
		{
			rootDir[i].num = -1;
			//Initializing the names of the files to nothing
			memset(rootDir[i].name, "", sizeof(rootDir[i].name));
		}

		//Writing the directory into the disk
		if(blocksForDir() < 0)
			printf("Cannot write the directory into disk.\n");
	
		//Occupying the directory entries in the bitmap table
		for(i = 0; i < NO_BLOCKS_DIR; i++) 
		{
			force_set_index(STR_DATABLOCKS+i);
		}

		//Initializing the 0th entry of fd_table
		fd_table[0].inodeIndex = 0;
		fd_table[0].inode = &in_table[0]; 
		fd_table[0].rwptr = 0;

		force_set_index(NUM_BLOCKS-1);

		//Writing the bitmap table into the disk
		if (blockForBitmap() < 0)
			printf("Cannot write the bitmap into disk.\n");
	}

	//If the disk has already been initialized
	else if(!fresh)
	{
		init_fdt();

		init_disk(GAYE_ABDELKADER_DISK, BLOCK_SIZE, NUM_BLOCKS);

		//Superblock
		readSuperb();
		force_set_index(0);

		//Inode
		readInode();
		for(int i = 0; i < NO_BLOCKS_ITB; i++)
		{
			force_set_index(i+1);
		}
		
		//Bitmap
		readBitmap();
		force_set_index(NUM_BLOCKS-1);
	}
}

int sfs_getnextfilename(char *fname)
{
	int result = 0;
	//If we are inside the last file in the directory, go back to the first file
	if(num_files == file_track)
	{
		file_track = 0;
	}
	else
	{	
		//Getting the name of the next file
		int index = 0;
		int i;
		for(i = 0; i < (NO_OF_INODES-1); i++)
		{
			//If the entry does not contain a file
			if(rootDir[i].num == -1)
			{
				if(index == file_track)
				{
					memcpy(fname, rootDir[i].name, sizeof(rootDir[i].name));
					break;
				}
				index++;
			}
		}
		file_track++;
		result = 1;
	}

	return result;
}

int sfs_getfilesize(const char* path)
{
	int result = -1;
	int i;
	for(i=0; i<(NO_OF_INODES-1); i++)
	{
		//Find where the file is in the directory and get its size from the inode table
		if(!strcmp(path, rootDir[i].name))
		{
			result = in_table[rootDir[i].num].size;
			break;
		}
	}
	if(result == -1)
		printf("File not found.");

	return result;
}


int sfs_fopen(char *name)
{
	int i;
	int inode_num = -1;
	int index = 0;

	//Checking if the file exists
	for(i=0; i<(NO_OF_INODES-1); i++) 
	{
		if(!strcmp(rootDir[i].name, name)) 
		{
			index = 1;
			inode_num = rootDir[i].num;
			break;
		}
	}

	//If the file exists, return its inode index
	if(index == 1) 
	{
		for(i=1; i<NO_OF_INODES; i++) 
		{
			if(fd_table[i].inodeIndex == inode_num) 
				return i;
		}
	} 

	else 
	{
		//Finding a free space in the directory
		int dir_i = -1;
		for(i=0; i<(NO_OF_INODES-1); i++) 
		{
			if (rootDir[i].num < 0) 
			{
				dir_i = i;
				break;
			}
		}

		//Cannot open the file if the directory is full
		if(dir_i == -1) 
		{
			printf("The directory is currently full.\n");
			return -1;
		}

		//Getting a free inode
		for(i=1; i<NO_OF_INODES; i++) 
		{
			if (in_table[i].size == -1) 
			{
				inode_num = i;
				break;
			}
		}

		strcpy(rootDir[dir_i].name, name);
		rootDir[dir_i].num = inode_num;

		in_table[inode_num].size = 0;
		//Writing the blocks for the inode table and the directory
		blocksForInodeT();
		blocksForDir();
	}

	//Checking for a inode table spot
	int fdt_i = -1;
	for(i=1; i<NO_OF_INODES; i++) 
	{
		if(fd_table[i].inodeIndex == -1) 
		{
			fdt_i = i;
			break;
		}
	}

	fd_table[fdt_i].inodeIndex = inode_num;
	fd_table[fdt_i].inode = &in_table[inode_num]; 

	//Allocating the new read_write pointer
	int ptr = in_table[inode_num].size;
	fd_table[fdt_i].rwptr = ptr;

	//Incrementing the total number of files opened
	num_files++;

	return fdt_i;
}

int sfs_fclose(int fileID) 
{
	//If the fileID is smaller than 0 (reserved for the root directory) or bigger than the maximum number of files
	if (fileID <= 0) 
		return -1;
	//If the fileID is bigger than the max number of files
	else if(fileID > (NO_OF_INODES-1))
		return -1;

	//Checking if the file is opened
	if(fd_table[fileID].inodeIndex == -1) 
	{
		printf("The file is not opened.\n");
		return -1;
	}

	//Changing the value of the file_descriptor table entry to empty
	fd_table[fileID].inodeIndex = -1;
	fd_table[fileID].inode = NULL;
	fd_table[fileID].rwptr = 0;

	//Decrementing the total number of files opened
	num_files--;

	return 0;
}

int sfs_fread(int fileID, char *buf, int length) 
{
	//This ensures correctness of parameters
	if(buf == NULL || length <= 0)
	{
        return -1;
	}

	//Check if file is open
	if(fd_table[fileID].inodeIndex == -1) 
	{
		printf("The file is not opened.\n");
		return -1;
	}

	return length;
}

int sfs_fwrite(int fileID, const char *buf, int length) 
{
	//Verifying for correct parameters
	if(buf == NULL || length < 0) 
	{
		printf("Error in the parameters.");
		return -1;
	}

	file_descriptor *curr = &fd_table[fileID];

	//Checking if the file has been opened
	if((*curr).inodeIndex == -1) 
	{
		printf("The file is not opened.");
		return -1;
	}

	inode_t *curr_inode = &in_table[(*curr).inodeIndex];

	//Checking if the inode has been connected
	if(curr_inode == -1)
	{
        return -1;
	}

	int ptr = (*curr).rwptr;

	int s_block_index = (ptr/BLOCK_SIZE);
	int s_offset = (ptr%BLOCK_SIZE);

	int ptr_end = ptr+length;

	int e_block_index = (ptr_end/BLOCK_SIZE);
	int e_offset = (ptr_end%BLOCK_SIZE);
	
	return length;
}

int sfs_fseek(int fileID, int loc) 
{
	//If the fileID is smaller than 0 (reserved for the root directory) or bigger than the maximum number of files
	if(fileID <= 0) 
		return -1;
	//If the fileID is bigger than the max number of files
	else if(fileID > (NO_OF_INODES-1))
		return -1;

	//Checking if the file is opened
	if(fd_table[fileID].inodeIndex == -1) 
	{
		printf("The file is not opened.\n");
		return -1;
	}

	//The statements below determine where to start the pointer
	int size_of_inode = in_table[fd_table[fileID].inodeIndex].size;

	if(loc < 0) 
		fd_table[fileID].rwptr = 0;
	else if(loc > size_of_inode)
		fd_table[fileID].rwptr = size_of_inode;
	else
		fd_table[fileID].rwptr = loc;

	return 0;
}

int sfs_remove(char *file) 
{
	return 0;
}

