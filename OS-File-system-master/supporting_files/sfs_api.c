
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
#define SIZE_ROOTDIR NUM_INODES-1 
#define BLOCKS_INODE ((NUM_INODES)*(sizeof(inode_t))/BLOCK_SIZE + (((NUM_INODES)*(sizeof(inode_t)))%BLOCK_SIZE > 0))
#define BLOCKS_ROOTDIR ((NUM_INODES-1)*(sizeof(directory_entry))/BLOCK_SIZE + (((NUM_INODES-1)*(sizeof(directory_entry)))%BLOCK_SIZE > 0))
#define START_OF_DATABLOCKS (1+BLOCKS_INODE) // superblock + BLOCKS_INODE
#define BITMAP_ROW_SIZE (NUM_BLOCKS/8) // this essentially mimcs the number of rows we have in the bitmap. we will have 128 rows. 
#define NUM_ADDRESSES_INDIRECT (BLOCK_SIZE/sizeof(int))
#define MAX_BYTES 30000 //As in the test file

struct superblock_t superblock;
struct file_descriptor fd_table[NUM_INODES]; 
struct inode_t in_table[NUM_INODES];
struct directory_entry rootDir[SIZE_ROOTDIR];
void* buffer;


/* macros */
#define FREE_BIT(_data, _which_bit) \
    _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
    _data = _data & ~(1 << _which_bit)

//initialize all bits to high
uint8_t bit_map[BITMAP_ROW_SIZE] = { [0 ... BITMAP_ROW_SIZE - 1] = UINT8_MAX };

// Write inode to disk
int writeInode() 
{ 
	buffer = (void*) malloc(BLOCKS_INODE*BLOCK_SIZE);
	//Fill the block with zeros to identify when the real data ends
	memset(buffer, 0, BLOCKS_INODE*BLOCK_SIZE);
	//Copy actual data
	memcpy(buffer, in_table, (BLOCKS_INODE)*(sizeof(inode_t)));
	//Start at block 1, 0 is used for superblock
	int status = write_blocks(1, BLOCKS_INODE, buffer);
	free(buffer);
	return status;
}

// Write rootDir to disk
int writeRootDir() 
{ 
	buffer = (void*) malloc(BLOCKS_ROOTDIR*BLOCK_SIZE);
	memset(buffer, 0, BLOCKS_ROOTDIR*BLOCK_SIZE);
	memcpy(buffer, rootDir, (SIZE_ROOTDIR)*(sizeof(directory_entry)));
	int status = write_blocks(START_OF_DATABLOCKS, BLOCKS_ROOTDIR, buffer);
	free(buffer);
	return status;
}

// Write Bit map to disk
int writeBitMap() 
{ 
	buffer = (void*) malloc(BLOCK_SIZE);
	//Set every bit to 1 (free)
	memset(buffer, 1, BLOCK_SIZE);
	//Copy all the used bits from the bitmap
	memcpy(buffer, bit_map, (BITMAP_ROW_SIZE)*(sizeof(uint8_t)));
	int status = write_blocks(NUM_BLOCKS - 1, 1, buffer);
	free(buffer);
	return status;
}

int writeSuper()
{
	//Write superblock to disk
	buffer = (void*) malloc(BLOCK_SIZE);
	memset(buffer, 0, BLOCK_SIZE);
	memcpy(buffer, &superblock, sizeof(superblock_t));
	int status = write_blocks(0, 1, buffer);
	
	free(buffer);

	return status;
}

// init file descriptors
void init_fdt()
{
	//Init fd_table[0] later, after in_table has been init
	//fd_table[0] = (file_descriptor) {0, &in_table[0], 0};

	for (int i = 1; i < NUM_INODES; i++) {
		fd_table[i].inodeIndex = -1;
		fd_table[i].inode = NULL; 
		fd_table[i].rwptr = 0;

		//fd_table[i] = (file_descriptor) {-1, NULL, 0};
	}
}

// init inode table 
void init_int()
{
	
	//Set in_table[0] for the root 
	in_table[0].mode = 777;
	in_table[0].link_cnt = 0;
	in_table[0].uid = 0;
	in_table[0].gid = 0;
	in_table[0].size = 0;
	for(int i = 0; i < 12; i++)
	{
		if(i < BLOCKS_ROOTDIR)
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

void init_super()
{	
	//superblock = (superblock_t) {0xACBD0005, BLOCK_SIZE, NUM_BLOCKS, NUM_INODES, 0};
	superblock.magic = 0xACBD005;
	superblock.block_size = BLOCK_SIZE;
	superblock.fs_size = NUM_BLOCKS*BLOCK_SIZE;
	superblock.inode_table_len = NUM_INODES;
	superblock.root_dir_inode = 0;	
}

void mksfs(int fresh) 
{
	int disk_status;

	if (fresh)
	{
		init_fdt();
		init_int();
		init_super();

		disk_status = init_fresh_disk(GAYE_ABDELKADER_DISK, BLOCK_SIZE, NUM_BLOCKS);

		//Write super
		if(writeSuper() < 0)
		{
			printf("Writing superblock to disk was unsuccessful\n");
		}
		// occupy superblock in bit map
		force_set_index(0);

		//Write inode
		if (writeInode() < 0)
		{
			printf("Writing inode to disk was unsuccessful\n");
		}
		//Occupy inodes in bit map
		for (int i = 0; i < BLOCKS_INODE; i++)
		{
			//0 is used by the superblock
			force_set_index(i+1);
		}

		// init directory entries
		for (int i = 0; i < SIZE_ROOTDIR; i++) {
			rootDir[i].num = -1;
			//Set name to empty string
			memset(rootDir[i].name, '\0', sizeof(rootDir[i].name));
		}

		//Write root dir
		if (writeRootDir() < 0)
		{
			printf("Writing root directory to disk was unsuccessful\n");
		}
		//Occupy root dir block
		for (int i = 0; i < BLOCKS_ROOTDIR; i++) 
		{
			force_set_index(START_OF_DATABLOCKS+i);
		}

		//Init fd[0] with in_table[0]
		fd_table[0] = (file_descriptor) { 0, &in_table[0], 0 };

		//Occupt free bit block
		force_set_index(NUM_BLOCKS - 1);
		//Write free bit map
		if (writeBitMap() < 0)
		{
			printf("Writing bit map to disk unsuccessful \n");
		}		
	}	
	else if (!fresh)
	{
		init_fdt();

		disk_status = init_disk(GAYE_ABDELKADER_DISK, BLOCK_SIZE, NUM_BLOCKS);

		//Read superblock from disk
		buffer = (void*) malloc(BLOCK_SIZE);
		memset(buffer, 0, BLOCK_SIZE);
		read_blocks(0, 1, buffer);
		memcpy(&superblock, buffer, sizeof(superblock_t));
		free(buffer);
		//Occupy superblock in bit map
		force_set_index(0);

		//Read in_table from disk
		buffer = (void*) malloc(BLOCKS_INODE*BLOCK_SIZE);
		memset(buffer, 0, BLOCKS_INODE*BLOCK_SIZE);
		read_blocks(1, BLOCKS_INODE, buffer);
		memcpy(in_table, buffer, BLOCKS_INODE*sizeof(inode_t));
		free(buffer);
		//Occupy in_table in bit map
		for (int i = 0; i < BLOCKS_INODE; i++)
		{
			force_set_index(i+1);
		}

		//Read bit map from disk
		buffer = (void*) malloc(BLOCK_SIZE);
		memset(buffer, 1, BLOCK_SIZE);
		read_blocks(NUM_BLOCKS-1, 1, buffer);
		memcpy(bit_map, buffer, (BITMAP_ROW_SIZE)*(sizeof(uint8_t)));
		free(buffer);
		//Occupy bit map in bit map
		force_set_index(NUM_BLOCKS-1);

	}
	else 
	{
		printf("%i is not a valid value for fresh", fresh);
	}

	if(disk_status == -1)
	{
		printf("Disk could not be initialized. Rerunning mksfs");
		//If disk failed to initialize, rerun the mksfs function
		mksfs(fresh); 
	}
}

int sfs_getnextfilename(char *fname)
{

}
int sfs_getfilesize(const char* path)
{

}
int sfs_fopen(char *name)
{
	int inodeIndex = -1;
	int exists = 0;
	for (int i = 0; i < SIZE_ROOTDIR; i++) 
	{
		// printf("ROOT DIR NAME: %s\n", rootDir[i].name);
		// printf("INPUT NAME: %s\n", name);
		if (!strcmp(rootDir[i].name, name)) 
		{
			exists = 1;
			inodeIndex = rootDir[i].num;
			//return inodeIndex;
			break;
		}
	}

	if (exists) 
	{
		//printf("EXISTS\n");
		//Check if the file descriptor has already been opened
		for (int i = 1; i < BLOCKS_INODE; i++) 
		{
			//printf("fd_table[i]: %d\n", fd_table[i].inodeIndex);
			//printf("inodeIndex: %d\n", inodeIndex);
			if (fd_table[i].inodeIndex == inodeIndex) 
			{
				//printf("inodeIndex here: %d\n", inodeIndex);
				return i;
			}
		}
	} 
	else 
	{

		//Find empty space in directory
		int dirIndex = -1;
		for (int i = 0; i < SIZE_ROOTDIR; i++) 
		{
			if (rootDir[i].num == -1)
			{
				dirIndex = i;
				break;
			}
		}
		//Print error is no empty space is found
		if (dirIndex == -1) 
		{
			printf("No empty space found in root directory\n");
			return -1;
		}

		//Check for unused inode in in_table
		for (int i = 1; i < BLOCKS_INODE; i++) 
		{
			//printf("1 in_table[i].size: %d\n", in_table[i].size);
			if (in_table[i].size == -1) 
			{
				inodeIndex = i;
				break;
			}
		}
		//Print error if no inode is found
		if (inodeIndex == -1) 
		{
			printf("Unused inode not found\n");
			return -1;
		}

		//Update root direcotry with the correct name and inode index
		strcpy(rootDir[dirIndex].name, name);
		rootDir[dirIndex].num = inodeIndex;
		//printf("Inode index i put it in: %d\n", inodeIndex);

		//Write inode to disk
		if (writeInode() < 0)
		{
			printf("Writing the inode table to disk was unsuccessful\n");
		}
		//Write root dir to disk
		if (writeRootDir() < 0)
		{
			printf("Writing root directory to disk was unsuccessful\n");
		}
	}

	//Find slot in fdt
	int i;
	int fdIndex = -1;
	for (i = 1; i < BLOCKS_INODE; i++) 
	{ 
		//If slot is found get the index
		if (fd_table[i].inodeIndex == -1) 
		{
			fdIndex = i;
			break;
		}
	}
	//If no available slot is found, print error
	if (i == BLOCKS_INODE) 
	{ 
		printf("No slot found in fdt\n");
		return -1;
	}

	//Update fdt
	int rwptr = in_table[inodeIndex].size;
	fd_table[fdIndex] = (file_descriptor) {inodeIndex, &in_table[inodeIndex], rwptr};

	//Set size of used inode table to 0 from -1 to indicate that a file has been opened at that location
	//Unless there is already data
	//printf("2 in_table[inodeIndex].size: %d\n", in_table[inodeIndex].size);
	if(in_table[inodeIndex].size == -1)
	{
		in_table[inodeIndex].size = 0;
		printf("4 in_table[inodeIndex].size: %d\n", in_table[inodeIndex].size);
	}
	//printf("3 in_table[inodeIndex].size: %d\n", in_table[inodeIndex].size);

	//totalFiles++;

	//printf("sdighsdihg\n");
	return fdIndex;
}

int sfs_fclose(int fileID) 
{

}

int sfs_fread(int fileID, char *buf, int length) 
{
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
	else
	{

	}

	return length;
}

int sfs_fwrite(int fileID, const char *buf, int length) 
{
	//Check to make sure length is not negative and buff is not null
	if (buf == NULL || length < 0) 
	{
		printf("Length cannot be negative");
		return -1;
	}

	//Open file descriptor for the fileID
	file_descriptor *currentFD = &fd_table[fileID];

	// //Check if there is enough space in the file to write
	// int avail = MAX_BYTES - currentFD->rwptr;
 //    if(avail <= 0)
 //    {
 //        return -1;
 //    }

 //    //Set space to either the length to write or the available space left in the file
 //    int space = 0;
 //    if(avail >= length)
 //    {
 //        space = length;
 //    }
 //    else
 //    {
 //        space = avail;
 //    }

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

	int counter = 0;
	int size_buf = sizeof(char)*strlen(buf);
	printf("size_buf = %d\n", size_buf);

	uint64_t point = currentFD->rwptr;
	printf("point = %d\n", point);
	
}

int sfs_fseek(int fileID, int loc) 
{

}

int sfs_remove(char *file) 
{


}

