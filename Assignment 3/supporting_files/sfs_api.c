
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
#define NO_FILES (NO_OF_INODES-1)


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

//Initializing the inode table
void init_int()
{
	int i, j;
	for(i=0; i < NO_OF_INODES; i++)
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

void mksfs(int fresh) 
{
	if(fresh)
	{

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

