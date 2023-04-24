/*
Implementation of SimpleFS.
Make your changes here.
*/

#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdint.h>

extern struct disk *thedisk;

int fs_format()
{
	return 0;
}

void fs_debug()
{
	union fs_block block;

	disk_read(thedisk,0,block.data);
	int numBlocks = block.super.nblocks;
	int numInodes = block.super.ninodeblocks;

	if(block.super.magic == FS_MAGIC){
		printf("superblock:\n");
	} else {
		printf("error, invalid magic number\n");
		return;
	}
	if(disk_size() == block.super.nblocks) {
		printf("    %d blocks\n",block.super.nblocks);
	} else {
		printf("error, superblock has wrong nblocks\n");
		return;
	}
	if(block.super.ninodeblocks == 1 + (int) block.super.nblocks / 10){
		printf("    %d inode blocks\n",block.super.ninodeblocks);
	} else {
		printf("error, incorrect number of inode blocks\n");
		return;
	}
	if(block.super.ninodes == INODES_PER_BLOCK * block.super.ninodeblocks) {
		printf("    %d inodes\n",block.super.ninodes);
	} else {
		printf("error, incorrect number of inodes\n");
		return;
	}


	// for each inode block
	for(int k=0; k<numInodes; k++) {
		disk_read(thedisk,k+1,block.data);
		// for each inode
		for (int i=0; i<INODES_PER_BLOCK; i++){
			if(block.inode[i].isvalid){
				printf("inode %i:\n", i);
			} else {
				continue;
			}
			if(block.inode[i].size > (numBlocks - 2) * DISK_BLOCK_SIZE || block.inode[i].size <= 0) {
				printf("error, invalid inode size\n");
				return;
			} else {
				printf("    size: %d bytes\n",block.inode[i].size);
			}
			if(block.inode[i].direct[0]){
				printf("    direct blocks: ");
				// for each direct block
				for(int j=0; j<POINTERS_PER_INODE; j++){
					if(block.inode[i].direct[j]) {
						printf("%d ",block.inode[i].direct[j]);
					}
				}
				printf("\n");
			}
			if(block.inode[i].indirect) {
				printf("    indirect block: %d\n",block.inode[i].indirect);
				disk_read(thedisk, block.inode[i].indirect, block.data);
				printf("    indirect data blocks: ");
				// for each pointer in the indirect block
				for(int l=0; l<POINTERS_PER_BLOCK; l++){
					if(block.pointers[l]) {
						printf("%d ", block.pointers[l]);
					}
				}
				printf("\n");
			}
		}
	}

}

int fs_mount()
{
	return 0;
}

int fs_create()
{
	return 0;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return 0;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
