#include "filesys/inode_utils.h"
#include <debug.h>
#include <list.h>
#include <round.h>
#include <stdio.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "threads/malloc.h"

static size_t size_error = -1;

void zero_and_write_block(block_sector_t block) {
    /* Fills allocated blocks with 0's */
    static char zeros[BLOCK_SECTOR_SIZE];
    block_write(fs_device, block, zeros);
}

/* Allocates nonconsecutive blocks of memory from free map */
bool allocate_nonconsec(size_t count, block_sector_t *block_list, size_t start) {
    size_t i = 0;
    while (i < count) {
        bool success = free_map_allocate(1, &block_list[start + i]);
        if (!success) {
            printf(
                "HI THERE! I'M HAVING AN ISSUE :( PLEASE FIND ME IN "
                "ALLOCATE_NONCONSEC\n");
            return false;
        }
        i++;
    }

    return true;
}

/* Releases nonconsecutive blocks of memory from free map */
void release_nonconsec(size_t count, block_sector_t *block_list) {
    size_t i = 0;
    while (i < count) {
        free_map_release(block_list[i], 1);
        i++;
    }
}

bool inode_disk_extend(struct inode_disk *disk_inode, size_t size) {
    size_t curr_num_blocks = disk_inode->num_sectors;
    size_t new_num_blocks = bytes_to_sectors(disk_inode->length + size);

    /* How many blocks we need to allocate for given size*/
    size_t needed_blocks = new_num_blocks - curr_num_blocks;

    /* Calculates how many direct pointers we need  */
    size_t needed_direct_blocks = needed_blocks;

    /* Checks to see how many indirect blocks we need to allocate */
    size_t needed_indirect_blocks =
        needed_blocks > NUM_DIRECT ? (needed_blocks - NUM_DIRECT) / NUM_INDIRECT : 0;

    /* Checks if we need to allocate space for the doubly indirect pointers block */
    size_t needed_doubly_indirect_blocks = needed_blocks > NUM_DIRECT + NUM_INDIRECT ? 1 : 0;

    // printf("HELLLLOOOOOOOOOOOOOO\n");
    // printf("Size: %u\n", size);
    // printf("Current number of blocks: %u\n", curr_num_blocks);
    // printf("I want this many blocks: %u\n", new_num_blocks);
    // printf("I need to allocate this many blocks: %u\n", needed_blocks);
    // printf("Needed Direct Blocks: %u\n", needed_direct_blocks);
    // printf("Needed Indirect Blocks: %u\n", needed_indirect_blocks);
    // printf("Needed Doubly Indirect Blocks: %u\n", needed_doubly_indirect_blocks);
    // printf("NUM_DIRECT: %u\n", NUM_DIRECT);
    // printf("NUM_INDIRECT: %u\n", NUM_INDIRECT);

    /* Do we have enough space to allocate everything? */
    if (!free_map_available_space(needed_direct_blocks + needed_indirect_blocks +
                                  needed_doubly_indirect_blocks)) {
        return false;
    }

    /* Extends the inode disk */
    size_t remaining_blocks = needed_blocks;
    remaining_blocks = allocate_direct_blocks(disk_inode, remaining_blocks);
    remaining_blocks = allocate_indirect_blocks(disk_inode, remaining_blocks);
    remaining_blocks = allocate_doubly_indirect_blocks(disk_inode, remaining_blocks);

    /* Updates length with newly allocated blocks if allocation was successful */
    if (remaining_blocks == 0) {
        disk_inode->length += size;
    }
    /* Fails if not all blocks were able to be allocated */
    return remaining_blocks == 0;
}

size_t allocate_direct_blocks(struct inode_disk *disk_inode, size_t remaining_blocks) {
    size_t curr_num_blocks = disk_inode->num_sectors;

    /* Skips if direct blocks are full, no more blocks needed to allocate, or previous failed */
    if (curr_num_blocks > NUM_DIRECT || remaining_blocks == 0 || remaining_blocks == size_error) {
        return remaining_blocks;
    }

    /* Allocates blocks for the direct blocks list or fails */
    size_t num_to_add = curr_num_blocks + remaining_blocks > NUM_DIRECT
                            ? NUM_DIRECT - curr_num_blocks
                            : remaining_blocks;
    if (!allocate_nonconsec(num_to_add, disk_inode->direct, curr_num_blocks)) {
        return -1;
    }

    /* Fills allocated blocks with 0's */
    size_t i;
    for (i = curr_num_blocks; i < curr_num_blocks + num_to_add; i++) {
        zero_and_write_block(disk_inode->direct[i]);
    }

    /* Updates num_sectors with recently allocated blocks */
    disk_inode->num_sectors += num_to_add;

    /* Returns how many blocks are left to allocate */
    return remaining_blocks - num_to_add;
}

size_t allocate_indirect_blocks(struct inode_disk *disk_inode, size_t remaining_blocks) {
    size_t curr_num_blocks = disk_inode->num_sectors - NUM_DIRECT;

    /* Skips if indirect blocks are full, no more blocks needed to allocate, or previous failed */
    if (curr_num_blocks > NUM_INDIRECT || remaining_blocks == 0 || remaining_blocks == size_error) {
        return remaining_blocks;
    }

    /* Allocates the indirect pointers block if first time entering */
    if (curr_num_blocks == 0) {
        bool success = free_map_allocate(1, &disk_inode->indirect);
        if (!success) {
            printf("Failed to allocate indirect block");
            return -1;
        }
        zero_and_write_block(disk_inode->indirect);
    }

    /* Loads indirect block list from storage */
    block_sector_t indirect_list[NUM_INDIRECT];
    block_read(fs_device, disk_inode->indirect, indirect_list);

    /* Allocates blocks for the indirect blocks list or fails */
    size_t num_to_add = curr_num_blocks + remaining_blocks > NUM_INDIRECT
                            ? NUM_INDIRECT - curr_num_blocks
                            : remaining_blocks;
    if (!allocate_nonconsec(num_to_add, indirect_list, curr_num_blocks)) {
        return -1;
    }

    /* Fills allocated blocks with 0's */
    size_t i;
    for (i = curr_num_blocks; i < curr_num_blocks + num_to_add; i++) {
        zero_and_write_block(indirect_list[i]);
    }

    /* Updates num_sectors with recently allocated blocks */
    disk_inode->num_sectors += num_to_add;

    /* Returns how many blocks are left to allocate */
    return remaining_blocks - num_to_add;
}

size_t allocate_doubly_indirect_blocks(struct inode_disk *disk_inode, size_t remaining_blocks) {
    size_t curr_num_blocks = disk_inode->num_sectors - NUM_DIRECT - NUM_INDIRECT;

    /* Skips if no more blocks needed to allocate or previous failed */
    if (remaining_blocks == 0 || remaining_blocks == size_error) {
        return remaining_blocks;
    }

    /* Allocates the doubly-indirect pointers block if first time entering */
    if (curr_num_blocks == 0) {
        bool success = free_map_allocate(1, &disk_inode->doubly_indirect);
        if (!success) {
            printf("Failed to allocate doubly-indirect block");
            return -1;
        }
        zero_and_write_block(disk_inode->doubly_indirect);
    }

    /* Loads doubly-indirect block list from storage */
    block_sector_t doubly_indirect_list[NUM_INDIRECT];
    block_read(fs_device, disk_inode->doubly_indirect, doubly_indirect_list);

    size_t curr_indirect_block = curr_num_blocks / NUM_INDIRECT;
    curr_num_blocks -= curr_indirect_block * NUM_INDIRECT;

    while (remaining_blocks > 0) {
        /* Allocates the doubly-indirect pointers block if first time entering */
        if (curr_num_blocks == 0) {
            bool success = free_map_allocate(1, &doubly_indirect_list[curr_indirect_block]);
            if (!success) {
                printf("Failed to allocate doubly-indirect subblock");
                return -1;
            }
            zero_and_write_block(disk_inode->doubly_indirect);
        }

        /* Loads doubly-indirect subblock list from storage */
        block_sector_t subblock_list[NUM_INDIRECT];
        block_read(fs_device, doubly_indirect_list[curr_indirect_block], subblock_list);

        /* Allocates blocks for the subblock list or fails */
        size_t num_to_add = curr_num_blocks + remaining_blocks > NUM_INDIRECT
                                ? NUM_INDIRECT - curr_num_blocks
                                : remaining_blocks;
        if (!allocate_nonconsec(num_to_add, subblock_list, curr_num_blocks)) {
            return -1;
        }

        /* Fills allocated blocks with 0's */
        size_t i;
        for (i = curr_num_blocks; i < curr_num_blocks + num_to_add; i++) {
            zero_and_write_block(subblock_list[i]);
        }

        /* Updates num_sectors with recently allocated blocks */
        disk_inode->num_sectors += num_to_add;

        /* Continues to next indirect block */
        remaining_blocks -= num_to_add;
        curr_num_blocks -= NUM_INDIRECT;
        curr_indirect_block++;
    }

    /* Returns how many blocks are left to allocate */
    return remaining_blocks;
}
