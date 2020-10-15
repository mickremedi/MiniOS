#ifndef FILESYS_INODE_UTILS_H
#define FILESYS_INODE_UTILS_H

#include <debug.h>
#include <round.h>
#include <stdbool.h>
#include <stdio.h>
#include "devices/block.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/off_t.h"

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t bytes_to_sectors(off_t size) { return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE); }

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t byte_to_sector(const struct inode *inode, off_t pos) {
    size_t sector_num = pos / BLOCK_SECTOR_SIZE;

    ASSERT(inode != NULL);
    if (pos > inode->data.length) {
        return -1;
    }

    if (sector_num < NUM_DIRECT) {
        return inode->data.direct[sector_num];
    }

    sector_num -= NUM_DIRECT;

    if (sector_num < NUM_INDIRECT) {
        /* Loads indirect block list from storage */
        block_sector_t indirect_list[NUM_INDIRECT];
        block_read(fs_device, inode->data.indirect, indirect_list);
        return indirect_list[sector_num];
    }

    sector_num -= NUM_INDIRECT;

    /* Loads doubly-indirect block list from storage */
    block_sector_t doubly_indirect_list[NUM_INDIRECT];
    block_read(fs_device, inode->data.doubly_indirect, doubly_indirect_list);

    /* Finds which indirect block the sector is in */
    size_t curr_indirect_block = sector_num / NUM_INDIRECT;

    /* Loads doubly-indirect subblock list from storage */
    block_sector_t subblock_list[NUM_INDIRECT];
    block_read(fs_device, doubly_indirect_list[curr_indirect_block], subblock_list);

    /* Finds which direct block the sector is in */
    sector_num = sector_num % NUM_INDIRECT;

    return subblock_list[sector_num];
}

void zero_and_write_block(block_sector_t block);
bool allocate_nonconsec(size_t count, block_sector_t *block_list, size_t start);
void release_nonconsec(size_t count, block_sector_t *block_list);
bool inode_disk_extend(struct inode_disk *disk_inode, size_t size);

size_t allocate_direct_blocks(struct inode_disk *disk_inode, size_t remaining_blocks);
size_t allocate_indirect_blocks(struct inode_disk *disk_inode, size_t remaining_blocks);
size_t allocate_doubly_indirect_blocks(struct inode_disk *disk_inode, size_t remaining_blocks);

void release_inode(struct inode *inode);

#endif /* filesys/inode_utils.h */
