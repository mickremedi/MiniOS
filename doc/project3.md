# Project 3: File Systems

## **Group Members**

- Ahmad Jawaid - *ahmadjawaid@berkeley.edu*
- Michael Remediakis - *m_remedi@berkeley.edu*
- Sarmad Farooq - *sarmadf@berkeley.edu*
- Joseph Zakher - *joseph.zakher@berkeley.edu*

# Task 1: Buffer Cache

## Overview of Task

1. Cache individual disk blocks
    1. Should be able to respond to reads with this cached data
    2. Can only have up to 64 blocks stored in cache at once, and since each block is of size 512 bytes while a page is 4KB, that would be 8 pages needed
    3. Needs a scheduler to manage which files are needed currently and replace blocks in the cache
2. Write-back caching - Have all disk operations make changes to the cached data
3. When it's time to remove the cache block or shutdown the system, the cached data will save into the disk, but only one disk write is allowed at a time

## Data Structures

    struct hash *cache_list;
    struct lock cache_lock;
    
    struct block {
      ...
      int reference_count;
      ...
    }
    
    void init_cache();
    void get_block(block_sector_t index);
    void drop_block(block_sector_t index);

## Algorithm

We will create 3 functions to build the caching system:

- [ ]  `void init_cache()` - initializes the caching system
    1. Use `palloc_get_multiple()` to allocate 8 contiguous pages of memory to represent a buffer cache
    2. Use `cache_list` to map to create a mapping of each block to their respective address in a page
    3. Initialize `cache_lock` to enforce that only one process is allowed to get or drop a block from the cache at a time, 
- [ ]  `void *get_block(block_sector_t index)` - gets a block from the cache
    1. Aquire the `cache_lock`
    2. With the index modded by 64, use the `cache_list` to try to get the appropriate block.
    3. Use the alarm clock's algorithm to get an unused block in the cache's memory and call `free_block` on it. If there are no unused blocks, then have the process wait for a block to get unused.
    4. Increment the block's reference count
    5. Call the block's read method
    6. Return the address to the acquired block
- [ ]  `void free_block(block_sector_t index)` - frees a block from the cache
    1. Decrements the block's reference count
    2. If the reference count reaches 0
        1. Call the block's write method to save the memory from the block in the appropriate device
        2. Clear the memory of the block

## Synchronization

The main synchronization decision we decided to make was for there to be a lock on the caching operations. The reason for this was that we didn't want to risk multiple processes trying to modify the location in the cache at the same time. One big situation that could occur for example would be if there is an empty block in the cache and two processes are trying to allocate different blocks to that same location. 

## Rationale

Allocating the memory for the cache system was simple enough because of the nice sizes we had to work with. We decided to use a list because of the fact that the memory we are working with is supposed to be represented as contiguous chunks, plus we could use the block index to easily determine where it a block should go.

# Task 2: Extensible Files

### Data Structures & Functions

In `inode.c`

    /* On-disk inode. Must be exactly BLOCK_SECTOR_SIZE bytes long. */
    struct inode_disk
      {
        block_sector_t start;               /* First data sector. */
        off_t length;                       /* File size in bytes. */
        unsigned magic;                     /* Magic number. */
        uint32_t unused[125];               /* Not used. */
    		block_sector_t direct;              /* Direct block. */
    	  block_sector_t indirect;            /* Indirect block. */
    	  block_sector_t doubly_indirect;     /* Doubly indirect block. */
      }

In `free-map.c`

    /* We will be editing this function to allocate non-consectutive
     blocks seemlessly */
    bool free_map_allocate (size_t cnt, block_sector_t *sectorp)
    

### Algorithm

By inspection of the current `free_map_allocate` function we see that it is very prone to external fragmentation because it only every tries to find *consecutive* blocks of memory. This is prone to failing a lot and not viable for our solution, we will instead be amending it to allocate non-consecutive blocks. This will be abstracted away so it will still appear to be the same. The algorithm is summarized below:

1. Check if there are enough non-consecutive blocks available, storing them as we go

    → We can immediately return false if this fails and clear our store

2. Initialize a list to store all out block selectors 
3. Add the block sectors we found to the array

We can now transition between blocks by checking the array for the next sector. While the blocks aren't consecutive, this design will achieve the same end goal. Our new `free_map_allocate` can also be used when writing past the EOF. It will successfully allocated blocks wihtout fragmentation and we can ammend our `inode_create` to store these as well.

### Syncrhonization

Since list operatios are always sucepitble to race conditions, we will always acquire a lock before storing the sectors. 

### Rationale

As mentioned earlier, the current implementation of  `free_map_allocate` is succeptible to external fragementation. By finding non consecutive blocks but abstracting it away we are able to achieve the same success with much more efficiency. Futhermore, keep direct pointer on our `inode-disk` will further help us optimize our accesses. 

# Task 3: Subdirectories

### Data Structures and Functions

In `inode.c`

    struct inode_disk{
    	uint32_t file_num;
    }
    

### Overview of Task

1. Implement support for hierarchal directory trees. //?????
2. Allow full path names to be longer than 14 characters by extending the limit of the basic file system.
3. In `follow_path()`, we will get all the names of the path using `get_next_part()`. If we encounter the forward slash (/) we can assume that it's an absolute path and traverse the latter from the root directory. Start at cwd otherwise.
4. Implement special file name support. For `..` we'll traverse to `parent_dir` in `inode_disk`.  We'll also use `dir_lookup()`.
5. Modify functions of `filesys.c` to be able to use `follow_path()`
6. Create the following file syscalls, where we aquire `io_lock` before any of the calls and release it after (same as in project 2):
    - `bool chdir (const char *dir)`
        1. Changes the current working directory of the process to dir.
        2. Returns true if successful, false otherwise
    - `bool mkdir (const char *dir)`
        1. Creates the directory named dir, which may be relative or absolute. 
        2. Returns true if successful, false on failure. Fails if dir already exists or if any directory name in dir, besides the last, does not already exist. That is, mkdir(“/a/b/c”) succeeds only if /a/b already exists and /a/b/c does not.
    - `bool readdir (int fd, char *name)`
        1. Reads a directory from file descriptor fd (which represent a directory).
        2. Stores the null-terminated file name in name if successful and returns true. Else returns false
    - `bool isdir (int fd)`
        1. Returns true if fd represent a directory and false if it's an ordinary file
    - `bool inumber (int fd)`
        1. Returns the inode number of the inode associated with fd (which represent a directory or an ordinary file).

    ### Synchronization

    We will not allow a process to delete a current working that is in use. We do this by making sure that the number of open count is greater than 0, if it is we will not allow it to be deleted. 

    ### Rationale

    `file_num` of `inode_disk` will keep track of the status of the directory: if it's empty and safe to remove.

# Additional Questions

Write Behind:

To implement a write behind feature mainly requires the system to periodically write the content of the blocks into the filesystem if they are "dirty". To do so, we mainly have to decide when to write to them. The simplest way to do this that could be thought of is to simply flush the memory every time we get to `x` amount of ticks from our timer system. The `x` can be determined through trial and error to determine which is best.

Read Ahead:

For read ahead, we would want to be able to predict which blocks we would want to use in the near future. For this, we could simply check if an allocated block has anymore data existing after what is currently accessed and pull that information into a list of blocks wanting to be read in the future.