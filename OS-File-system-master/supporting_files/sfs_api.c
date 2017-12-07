
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
#define NUM_BLOCKS 1024  //maximum number of data blocks on the disk.
#define NUM_INODES 100
#define NO_BLOCKS_ITB ((NUM_INODES)*(sizeof(inode_t))/BLOCK_SIZE + (((NUM_INODES)*(sizeof(inode_t)))%BLOCK_SIZE > 0))
#define NO_BLOCKS_DIR ((NUM_INODES-1)*(sizeof(directory_entry))/BLOCK_SIZE + (((NUM_INODES-1)*(sizeof(directory_entry)))%BLOCK_SIZE > 0))
#define START_OF_DATABLOCKS (1+NO_BLOCKS_ITB) // superblock + NO_BLOCKS_ITB
#define BITMAP_ROW_SIZE (NUM_BLOCKS/8) // this essentially mimcs the number of rows we have in the bitmap. we will have 128 rows. 
#define NUM_ADDRESSES_INDIRECT (BLOCK_SIZE/sizeof(int))
#define MAX_BYTES 30000 //As in the test file

struct superblock_t superb;
struct file_descriptor fd_table[NUM_INODES]; 
struct inode_t in_table[NUM_INODES];
struct directory_entry rootDir[(NUM_INODES-1)];
void* buffer;


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
	int num_blocks = write_blocks(0, 1, buffer);
	
	free(buffer);

	return num_blocks;
}

//Allocating blocks for the inodes
int blocksForInodeT() 
{ 
	buffer = (void*) malloc(NO_BLOCKS_ITB*BLOCK_SIZE);
	//Fill the block with zeros to identify when the real data ends
	memset(buffer, 0, NO_BLOCKS_ITB*BLOCK_SIZE);
	//Copy actual data
	memcpy(buffer, in_table, (NO_BLOCKS_ITB)*(sizeof(inode_t)));
	//Start at block 1, 0 is used for superblock
	int num_blocks = write_blocks(1, NO_BLOCKS_ITB, buffer);
	free(buffer);
	return num_blocks;
}

//Allocating blocks for the bitmap table
int blockForBitmap() 
{ 
	buffer = (void*) malloc(BLOCK_SIZE);
	//Set every bit to 1 (free)
	memset(buffer, 1, BLOCK_SIZE);
	//Copy all the used bits from the bitmap
	memcpy(buffer, f_bit_map, (BITMAP_ROW_SIZE)*(sizeof(uint8_t)));
	int num_blocks = write_blocks(NUM_BLOCKS - 1, 1, buffer);
	free(buffer);
	return num_blocks;
}

//Allocating blocks for the directory
int blocksForDir() 
{ 
	buffer = (void*) malloc(NO_BLOCKS_DIR*BLOCK_SIZE);
	memset(buffer, 0, NO_BLOCKS_DIR*BLOCK_SIZE);
	memcpy(buffer, rootDir, (NUM_INODES-1)*(sizeof(directory_entry)));
	int num_blocks = write_blocks(START_OF_DATABLOCKS, NO_BLOCKS_DIR, buffer);
	free(buffer);
	return num_blocks;
}


//Initializing the file descriptor table
void init_fdt()
{
	for (int i = 1; i < NUM_INODES; i++) 
	{
		fd_table[i].inodeIndex = -1;
		fd_table[i].inode = NULL; 
		fd_table[i].rwptr = 0;
	}
}

//Initializing the inode table 
void init_int()
{
	in_table[0].mode = 0777;
	in_table[0].link_cnt = 0;
	in_table[0].uid = 0;
	in_table[0].gid = 0;
	in_table[0].size = 0;
	for(int i = 0; i < 12; i++)
	{
		if(i < NO_BLOCKS_DIR)
		{
			in_table[0].data_ptrs[i] = START_OF_DATABLOCKS+i;
		}
		else
		{
			in_table[0].data_ptrs[i] = -1;
		}
	}
	in_table[0].indirectPointer = -1;


	//Start at 1 because zero is used for the superblock
	for (int i = 1; i < NUM_INODES; i++)
	{
		//Give everyone permission to read, write and execute
		in_table[i].mode = 777;
		in_table[i].link_cnt = -1;
		in_table[i].uid = -1;
		in_table[i].gid = -1;
		in_table[i].size = -1;
		for(int j = 0; j < 12; j++)
		{
			in_table[i].data_ptrs[j] = -1;
		}
		in_table[i].indirectPointer = -1;

	}
}

//Initializing the superblock
void init_super()
{	
	superb.magic = 0xACBD005;
	superb.block_size = BLOCK_SIZE;
	superb.fs_size = NUM_BLOCKS*BLOCK_SIZE;
	superb.inode_table_len = NUM_INODES;
	superb.root_dir_inode = 0;	
}

void mksfs(int fresh) 
{
	int status = -1;

	if(fresh)
	{
		int i;
		init_fdt();
		init_int();
		init_super();

		status = init_fresh_disk(GAYE_ABDELKADER_DISK, BLOCK_SIZE, NUM_BLOCKS);

		//Writing the superblock
		if(blockForSuperb() < 0)
			printf("Cannot write superblock to disk.\n");
	
		//Reserving the first block for the superblock
		force_set_index(0);

		//Writing the inodes
		if (blocksForInodeT() < 0)
			printf("Cannot write inodes to disk.\n");
	
		//Occupy inodes in bit map
		for (i = 0; i < NO_BLOCKS_ITB; i++)
		{
			force_set_index(i+1);
		}

		//Initializing the directory entries
		for (i = 0; i < (NUM_INODES-1); i++) 
		{
			rootDir[i].num = -1;
			//Set name to empty string
			memset(rootDir[i].name, '\0', sizeof(rootDir[i].name));
		}

		//Writing the directory
		if (blocksForDir() < 0)
			printf("Cannot write directory into disk.\n");
	
		//Occupying the root dir block
		for (i = 0; i < NO_BLOCKS_DIR; i++) 
		{
			force_set_index(START_OF_DATABLOCKS+i);
		}

		//Initializing the 0th entry of fd_table
		fd_table[0] = (file_descriptor) { 0, &in_table[0], 0 };

		force_set_index(NUM_BLOCKS-1);

		//Writing the bitmap table
		if (blockForBitmap() < 0)
			printf("Cannot write bitmap into disk.\n");
	}

	else if(!fresh)
	{
		init_fdt();

		status = init_disk(GAYE_ABDELKADER_DISK, BLOCK_SIZE, NUM_BLOCKS);

		//Read superblock from disk
		buffer = (void*) malloc(BLOCK_SIZE);
		memset(buffer, 0, BLOCK_SIZE);
		read_blocks(0, 1, buffer);
		memcpy(&superb, buffer, sizeof(superblock_t));
		free(buffer);
		//Occupy superblock in bit map
		force_set_index(0);

		//Read in_table from disk
		buffer = (void*) malloc(NO_BLOCKS_ITB*BLOCK_SIZE);
		memset(buffer, 0, NO_BLOCKS_ITB*BLOCK_SIZE);
		read_blocks(1, NO_BLOCKS_ITB, buffer);
		memcpy(in_table, buffer, NO_BLOCKS_ITB*sizeof(inode_t));
		free(buffer);
		//Occupy in_table in bit map
		for (int i = 0; i < NO_BLOCKS_ITB; i++)
		{
			force_set_index(i+1);
		}

		//Read bit map from disk
		buffer = (void*) malloc(BLOCK_SIZE);
		memset(buffer, 1, BLOCK_SIZE);
		read_blocks(NUM_BLOCKS-1, 1, buffer);
		memcpy(f_bit_map, buffer, (BITMAP_ROW_SIZE)*(sizeof(uint8_t)));
		free(buffer);
		//Occupy bit map in bit map
		force_set_index(NUM_BLOCKS-1);
	}

	if(status == -1)
	{
		printf("Disk could not be initialized. Rerunning it");
		mksfs(fresh); 
	}
}

int sfs_getnextfilename(char *fname)
{
	return 0;
}

int sfs_getfilesize(const char* path)
{
	int result = -1;
	int i;
	for(i=0; i<(NUM_INODES-1); i++)
	{
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
	// find file by name - iterate root directory
	int inode_num = -1;
	int index = 0;
	int i;

	//Checking if the file exists
	for (i=0; i<(NUM_INODES-1); i++) 
	{
		if (!strcmp(rootDir[i].name, name)) 
		{
			index = 1;
			inode_num = rootDir[i].num;
			break;
		}
	}

	//If the file exists, return its inode index
	if (index == 1) 
	{
		for (i=1; i<NUM_INODES; i++) 
		{
			if(fd_table[i].inodeIndex == inode_num) 
				return i;
		}
	} 

	else 
	{
		//Finding a free space in the directory
		int dir_index = -1;
		for (i=0; i<(NUM_INODES-1); i++) 
		{
			if (rootDir[i].num < 0) 
			{
				dir_index = i;
				break;
			}
		}

		if (dir_index == -1) 
		{
			printf("The directory is currently full.\n");
			return -1;
		}

		//Getting a free inode
		for (i=1; i<NUM_INODES; i++) 
		{
			if (in_table[i].size == -1) 
			{
				inode_num = i;
				break;
			}
		}

		strcpy(rootDir[dir_index].name, name);
		rootDir[dir_index].num = inode_num;
		in_table[inode_num].size = 0; // value that determines inode is used (-1)
	
		if (blocksForInodeT() < 0)
			printf("Cannot write to disk.");
		if (blocksForDir() < 0)
			printf("Cannot write to disk.");
	}

	//Getting the inode index from the file descriptor table
	int fdt_index = -1;
	for (i=1; i<NUM_INODES; i++) 
	{
		if (fd_table[i].inodeIndex == -1) 
		{
			fdt_index = i;
			break;
		}
	}

	//Allocating the new read_write pointer
	int ptr = in_table[inode_num].size;
	fd_table[fdt_index] = (file_descriptor) {inode_num, &in_table[inode_num], ptr};

	return fdt_index;
}

int sfs_fclose(int fileID) 
{
	//If the fileID is bigger than 0 (reserved for the root directory) or bigger than the maximum number of files
	if (fileID <= 0 || fileID > (NUM_INODES-1)) 
	{
		printf("Invalid file ID.\n");
		return -1;
	}

	//Checking if the file is opened
	if (fd_table[fileID].inodeIndex == -1) 
	{
		printf("The file is not opened.\n");
		return -1;
	}

	fd_table[fileID] = (file_descriptor) {-1, NULL, 0};

	return 0;
}

int sfs_fread(int fileID, char *buf, int length) 
{
	//This ensures correctness of parameters
	if(buf == NULL || length <= 0)
	{
        return -1;
	}

	file_descriptor *currentFD = &fd_table[fileID];

	//Check if file is open
	if (fd_table[fileID].inodeIndex == -1) 
	{
		printf("File is closed\n");
		return -1;
	}

	return length;
}

int sfs_fwrite(int fileID, const char *buf, int length) 
{
	//Verifying for correct parameters
	if (buf == NULL || length < 0) 
	{
		printf("Length cannot be negative");
		return -1;
	}

	//Open file descriptor for the fileID
	file_descriptor *currentFD = &fd_table[fileID];

	//Check that the file has been opened
	if ((*currentFD).inodeIndex == -1) 
	{
		printf("File has not yet been opened");
		return -1;
	}

	inode_t *currentInode = &in_table[(*currentFD).inodeIndex];

	//Check if the inode has been connected
	if(currentInode == -1)
	{
        return -1;
	}

	int ptr = (*currentFD).rwptr;

	int s_block_index = (ptr/BLOCK_SIZE);
	int s_offset = (ptr%BLOCK_SIZE);

	int ptr_end = ptr+length;

	int e_block_index = (ptr_end/BLOCK_SIZE);
	int e_offset = (ptr_end%BLOCK_SIZE);
	
	return length;
}

int sfs_fseek(int fileID, int loc) 
{
	//If the fileID is bigger than 0 (reserved for the root directory) or bigger than the maximum number of files
	if (fileID <= 0 || fileID > (NUM_INODES-1)) 
	{
		printf("Invalid file ID.\n");
		return -1;
	}

	//Checking if the file is opened
	if (fd_table[fileID].inodeIndex == -1) 
	{
		printf("The file is not opened.\n");
		return -1;
	}

	//The statements below determine where to start the pointer
	int size_of_inode = in_table[fd_table[fileID].inodeIndex].size;

	if (loc < 0) 
		fd_table[fileID].rwptr = 0;
	else if (loc > size_of_inode)
		fd_table[fileID].rwptr = size_of_inode;
	else
		fd_table[fileID].rwptr = loc;

	return 0;
}

int sfs_remove(char *file) 
{
	return 0;
}

