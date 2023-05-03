/*
Implementation of SimpleFS.
Make your changes here.
*/

#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

extern struct disk *thedisk;
int mounted = 0;
int * freeBlockBitmap;


int fs_format()
{
	if (!mounted) {
		union fs_block sblock;
		sblock.super.magic = FS_MAGIC;
		sblock.super.nblocks = disk_nblocks(thedisk);
		sblock.super.ninodeblocks = ceil(disk_nblocks(thedisk)/10);
		sblock.super.ninodes = INODES_PER_BLOCK * sblock.super.ninodeblocks; // Not sure about this
		disk_write(thedisk, 0, sblock.data);

		for(int i=0; i <sblock.super.ninodeblocks; i++){
			union fs_block inodeBlock;
			disk_read(thedisk,i+1,inodeBlock.data);

			for(int j=0; j<INODES_PER_BLOCK; j++){
				inodeBlock.inode[j].isvalid = 0;
				inodeBlock.inode[j].size = 0;
				// May not be needed // TODO
				for(int k=0; k<POINTERS_PER_INODE; k++){
					inodeBlock.inode[j].direct[k] = 0;
				}
				inodeBlock.inode[j].indirect = 0;
			}
			disk_write(thedisk,i+1,inodeBlock.data);
		}

		return 1; // what could fail?
	}
	printf("ALREADY MOUNTED\n");
	return 0; // already mounted
}

void fs_debug() // I may have missinterpreted. Not sure i am supposed to have errors for wrong number of inode blocks etc. TODO
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
	// if(block.super.ninodeblocks == ceil(block.super.nblocks / 10)){
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	// } else {
	// 	printf("error, incorrect number of inode blocks\n");
	// 	return;
	// }
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
			if(block.inode[i].size > (numBlocks - 2) * DISK_BLOCK_SIZE || block.inode[i].size < 0) {
				printf("error, invalid inode size\n");
				return;
			} else {
				printf("    size: %d bytes\n",block.inode[i].size);
			}
			if(block.inode[i].direct[0]){
				printf("    direct blocks: ");
				// for each direct block
				for(int j=0; j<POINTERS_PER_INODE; j++){
					if(block.inode[i].direct[j]) {  // this may not be correct
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
	union fs_block sBlock;
	disk_read(thedisk,0,sBlock.data);
	
	if(sBlock.super.magic != FS_MAGIC){
		printf("incorrect magic number\n");
		return 0;
	}
	if(sBlock.super.nblocks != disk_nblocks(thedisk)){
		printf("number of blocks do not match\n");
		return 0;
	}
	freeBlockBitmap = malloc(sizeof(int) * sBlock.super.nblocks);

	// set all free
	for(int i=0; i<sBlock.super.nblocks; i++){
		freeBlockBitmap[i] = 0;
	}
	int32_t nblocks = sBlock.super.nblocks;
	// super block takes the first block
	freeBlockBitmap[0] = 1;
	// inodes take the next <i> blocks
	for(int i=0; i<sBlock.super.ninodeblocks; i++){
		if(i+1 >= nblocks){
			printf("invalid inode block\n");
			return 0;
		}
		freeBlockBitmap[i+1] = 1;
	}
	// go through the inodes to see what blocks are taken (will be basing all off of the size of the inode)
	for(int i=0; i<sBlock.super.ninodeblocks; i++){
		union fs_block iBlock;
		disk_read(thedisk,i+1,iBlock.data);
		for(int j=0; j<INODES_PER_BLOCK; j++) {
			if(iBlock.inode[j].isvalid){
				int32_t uncountedSize = iBlock.inode[j].size;
				int blocksCounted = 0;
				while(uncountedSize > 0) {
					// if already counted all blocks an inode can possibly reach
					if(blocksCounted + 1 > POINTERS_PER_INODE + POINTERS_PER_BLOCK){
						printf("inode too large for this system\n");
						return 0;
					}
					// handle direct blocks first
					if(blocksCounted < POINTERS_PER_INODE){
						if(iBlock.inode[j].direct[blocksCounted] >= nblocks){
							printf("invalid direct block\n");
							return 0;
						}
						freeBlockBitmap[iBlock.inode[j].direct[blocksCounted]] = 1;
						blocksCounted ++;
						uncountedSize -= BLOCK_SIZE;
					} else { // indirect
						if(iBlock.inode[j].indirect >= nblocks){
							printf("invalid indirect block\n");
							return 0;
						}
						freeBlockBitmap[iBlock.inode[j].indirect] = 1;
						union fs_block indirBlock;
						disk_read(thedisk,iBlock.inode[j].indirect,indirBlock.data);
						// for each block pointed to in the indirect block
						if(indirBlock.pointers[blocksCounted-POINTERS_PER_INODE] >= nblocks) {
							printf("invalid indirect pointer\n");
							return 0;
						}
						freeBlockBitmap[indirBlock.pointers[blocksCounted-POINTERS_PER_INODE]] = 1;
						blocksCounted ++;
						uncountedSize -= BLOCK_SIZE;

					}
				}
			}
		}
	}


	printf("freeBlockBitmap: [%i", freeBlockBitmap[0]);

	for(int i=1; i<nblocks; i++){
		printf(", %i", freeBlockBitmap[i]);
	}
	printf("]\n");

	mounted = 1;

	return 1;
}

void inode_load(int inumber, struct fs_inode *inode)
{
	int inodeBlockNumber = (int) inumber / INODES_PER_BLOCK;
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
