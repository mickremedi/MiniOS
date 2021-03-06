#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <list.h>
#include <stdbool.h>
#include "devices/block.h"
#include "filesys/off_t.h"

struct bitmap;

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define NUM_DIRECT 120
#define NUM_INDIRECT 128
#define MAX_PAGES 8388608

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
    off_t length;                      /* File size in bytes. */
    int num_sectors;                   /* Number of sectors allocated */
    bool is_directory;                 /* States if a file is a directory */
    int directory_size;                /* The number of files in the directory */
    block_sector_t parent_directory;   /* Directory holding this file */
    block_sector_t direct[NUM_DIRECT]; /* Direct pointers. */
    block_sector_t indirect;           /* Indirect pointer. */
    block_sector_t doubly_indirect;    /* Doubly indirect pointer. */
    unsigned magic;                    /* Magic number. */
};

/* In-memory inode. */
struct inode {
    struct list_elem elem;  /* Element in inode list. */
    block_sector_t sector;  /* Sector number of disk location. */
    int open_cnt;           /* Number of openers. */
    bool removed;           /* True if deleted, false otherwise. */
    int deny_write_cnt;     /* 0: writes ok, >0: deny writes. */
    struct inode_disk data; /* Inode conent. */
};

void inode_init(void);
bool inode_create(block_sector_t, off_t, bool);
struct inode *inode_open(block_sector_t);
struct inode *inode_reopen(struct inode *);
block_sector_t inode_get_inumber(const struct inode *);
void inode_close(struct inode *);
void inode_remove(struct inode *);
off_t inode_read_at(struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at(struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write(struct inode *);
void inode_allow_write(struct inode *);
off_t inode_length(const struct inode *);
struct inode *inode_get_parent(struct inode *inode);
bool inode_add_to_dir(block_sector_t inode_sector, block_sector_t parent_sector);
bool inode_remove_from_dir(block_sector_t parent_sector);

#endif /* filesys/inode.h */
