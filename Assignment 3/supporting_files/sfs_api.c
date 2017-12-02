
#include "sfs_api.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <strings.h>
#include "disk_emu.h"
#define GAYE_ABDELKADER_DISK "sfs_disk.disk"
#define NUM_BLOCKS 1024  //maximum number of data blocks on the disk.
#define BLOCK_SIZE 1024
#define NO_OF_INODES 100 //maximum number of inodes
#define BITMAP_ROW_SIZE (NUM_BLOCKS/8) // this essentially mimcs the number of rows we have in the bitmap. we will have 128 rows. 


/* macros */
#define FREE_BIT(_data, _which_bit) \
    _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
    _data = _data & ~(1 << _which_bit)


//initialize all bits to high
uint8_t free_bit_map[BITMAP_ROW_SIZE] = { [0 ... BITMAP_ROW_SIZE - 1] = UINT8_MAX };

struct superblock_t superb;
struct file_descriptor fd_table[NO_OF_INODES];
struct inode_t in_table[NO_OF_INODES];
//Initializing the directory, containing the 99 files
struct directory_entry rootDir[NO_OF_INODES];

//Using a buffer to write to memory
void* buffer;


//Initializing the file descriptor table
void init_fdt()
{
	int i;
	for(i=0; i < NO_OF_INODES; i++)
	{
		fd_table[i]->inodeIndex = -1;
		fd_table[i]->inode = NULL;
		fd_table[i]->rwptr = 0;
	}
}

//Creating the inode entry for the root
void inode_root()
{
	in_table[0]->mode = -1;
	in_table[0]->link_cnt = -1;
	in_table[0]->uid = -1;
	in_table[0]->gid = -1;
	in_table[0]->size = sizeof(inode_t);

	int i;
	//Data pointers
	for(i=0; i < 12; i++)
	{

	}
}

//Initializing the inode table
void init_int()
{
	int i, j;
	//Leaving in_table[0] for the root directory
	for(i=1; i < NO_OF_INODES; i++)
	{
		in_table[i]->mode = -1;
		in_table[i]->link_cnt = -1;
		in_table[i]->uid = -1;
		in_table[i]->gid = -1;
		in_table[i]->size = -1;
		for(j=0; j < NO_OF_INODES; j++)
		{
			in_table[i]->data_ptrs[j] = -1;
		}
		in_table[i]->indirectPointer = -1;
	}
}

void init_superb()
{
	superb->magic = 0xABCD0005;
	superb->block_size = BLOCK_SIZE;
	superb->fs_size = NUM_BLOCKS*BLOCK_SIZE;
	superb->inode_table_len = NO_OF_INODES;
	superb->root_dir_inode = in_table[0];
}

//Writing a series of blocks for the inodes
void blocksForINodes()
{
	int no_blocks_inodet = ((NO_OF_INODES)*(sizeof(inode_t))/BLOCK_SIZE + (((NO_OF_INODES)*(sizeof(inode_t)))%BLOCK_SIZE > 0));
	buffer = (void*) malloc(no_blocks_inodet*BLOCK_SIZE);
	memcpy(buffer, in_table, NO_INODES*sizeof(inode_t));
	write_blocks(1, no_blocks_inodet, buffer);
	free(buffer);
}

//Writing a series of blocks for the data blocks
void blocksForData()
{
	buffer = (void*)malloc(NUM_BLOCKS*BLOCK_SIZE);
	write_blocks(/*TO ADD*/)
}

//Writing a block for the bitmap
void blockForBitmap()
{
	buffer = (void*)malloc(BLOCK_SIZE);
	memcpy(buffer, free_bit_map, sizeof(uint8_t)*BITMAP_ROW_SIZE);
	//The bitmap is 1 block
	write_blocks(/*TO ADD*/, 1, buffer);
	free(buffer);
}

void mksfs(int fresh) 
{
	if(fresh)
	{
		init_fresh_disk(GAYE_ABDELKADER_DISK, BLOCK_SIZE, NUM_BLOCKS);

		//Preparing the 1st inode for the root directory
		inode_root();

	}

	else
	{

	}
}
int sfs_getnextfilename(char *fname){

}
int sfs_getfilesize(const char* path){

}
int sfs_fopen(char *name){

}
int sfs_fclose(int fileID) {

}
int sfs_fread(int fileID, char *buf, int length) {
	
}
int sfs_fwrite(int fileID, const char *buf, int length) {

}
int sfs_fseek(int fileID, int loc) {

}
int sfs_remove(char *file) {


}

