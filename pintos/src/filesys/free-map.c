#include "filesys/free-map.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"

static struct file *free_map_file; /* Free map file. */
static struct bitmap *free_map;    /* Free map, one bit per sector. */
bool lock_acquire_helper(struct lock *lock);
void lock_release_helper(struct lock *lock, bool already_held);

/* Initializes the free map. */
void free_map_init(void) {
    free_map = bitmap_create(block_size(fs_device));
    if (free_map == NULL)
        PANIC("bitmap creation failed--file system device is too large");
    bitmap_mark(free_map, FREE_MAP_SECTOR);
    bitmap_mark(free_map, ROOT_DIR_SECTOR);
    lock_init(&map_lock);
}

bool lock_acquire_helper(struct lock *lock) {
    if (!lock_held_by_current_thread(lock)) {
        lock_acquire(lock);
        return false;
    }
    return true;
}

void lock_release_helper(struct lock *lock, bool already_held) {
    if (!already_held) {
        lock_release(lock);
    }
}

/* Allocates CNT consecutive sectors from the free map and stores
   the first into *SECTORP.
   Returns true if successful, false if not enough consecutive
   sectors were available or if the free_map file could not be
   written. */
bool free_map_allocate(size_t cnt, block_sector_t *sectorp) {
    bool already_held = lock_acquire_helper(&map_lock);
    block_sector_t sector = bitmap_scan_and_flip(free_map, 0, cnt, false);
    if (sector != BITMAP_ERROR && free_map_file != NULL &&
        !bitmap_write(free_map, free_map_file)) {
        bitmap_set_multiple(free_map, sector, cnt, false);

        sector = BITMAP_ERROR;
    }
    if (sector != BITMAP_ERROR) *sectorp = sector;
    lock_release_helper(&map_lock, already_held);
    return sector != BITMAP_ERROR;
}

/* Makes CNT sectors starting at SECTOR available for use. */
void free_map_release(block_sector_t sector, size_t cnt) {
    ASSERT(bitmap_all(free_map, sector, cnt));
    bool already_held = lock_acquire_helper(&map_lock);
    bitmap_set_multiple(free_map, sector, cnt, false);
    bitmap_write(free_map, free_map_file);
    lock_release_helper(&map_lock, already_held);
}

/* Opens the free map file and reads it from disk. */
void free_map_open(void) {
    bool already_held = lock_acquire_helper(&map_lock);
    free_map_file = file_open(inode_open(FREE_MAP_SECTOR));
    if (free_map_file == NULL) PANIC("can't open free map");
    if (!bitmap_read(free_map, free_map_file)) PANIC("can't read free map");
    lock_release_helper(&map_lock, already_held);
}

/* Writes the free map to disk and closes the free map file. */
void free_map_close(void) {
    bool already_held = lock_acquire_helper(&map_lock);
    file_close(free_map_file);
    lock_release_helper(&map_lock, already_held);
}

/* Creates a new free map file on disk and writes the free map to
   it. */
void free_map_create(void) {
    bool already_held = lock_acquire_helper(&map_lock);

    /* Create inode. */
    if (!inode_create(FREE_MAP_SECTOR, bitmap_file_size(free_map)))
        PANIC("free map creation failed");

    /* Write bitmap to file. */
    free_map_file = file_open(inode_open(FREE_MAP_SECTOR));
    if (free_map_file == NULL) PANIC("can't open free map");
    if (!bitmap_write(free_map, free_map_file)) PANIC("can't write free map");

    lock_release_helper(&map_lock, already_held);
}

/* Returns true if cnt number of sectors are available */
bool free_map_available_space(size_t cnt) {
    bool already_held = lock_acquire_helper(&map_lock);

    size_t totalAvailable =
        bitmap_count(free_map, 0, block_size(fs_device), false);
    lock_release_helper(&map_lock, already_held);
    return totalAvailable >= cnt;
}
