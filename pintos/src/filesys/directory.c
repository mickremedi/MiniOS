#include "filesys/directory.h"
#include <list.h>
#include <stdio.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* A directory. */
struct dir {
    struct inode *inode; /* Backing store. */
    off_t pos;           /* Current position. */
};

/* A single directory entry. */
struct dir_entry {
    block_sector_t inode_sector; /* Sector number of header. */
    char name[NAME_MAX + 1];     /* Null terminated file name. */
    bool in_use;                 /* In use or free? */
};

static int get_next_part(char part[NAME_MAX + 1], const char **srcp);

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool dir_create(block_sector_t sector, size_t entry_cnt) {
    return inode_create(sector, entry_cnt * sizeof(struct dir_entry), true);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *dir_open(struct inode *inode) {
    struct dir *dir = calloc(1, sizeof *dir);
    if (inode != NULL && dir != NULL && inode->data.is_directory) {
        dir->inode = inode;
        dir->pos = 0;
        return dir;
    } else {
        inode_close(inode);
        free(dir);
        return NULL;
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *dir_open_root(void) {
    return dir_open(inode_open(ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *dir_reopen(struct dir *dir) {
    return dir_open(inode_reopen(dir->inode));
}

/* Destroys DIR and frees associated resources. */
void dir_close(struct dir *dir) {
    if (dir != NULL) {
        inode_close(dir->inode);
        free(dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *dir_get_inode(struct dir *dir) {
    return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool lookup(const struct dir *dir, const char *name, struct dir_entry *ep, off_t *ofsp) {
    struct dir_entry e;
    size_t ofs;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e; ofs += sizeof e)
        if (e.in_use && !strcmp(name, e.name)) {
            if (ep != NULL) *ep = e;
            if (ofsp != NULL) *ofsp = ofs;
            return true;
        }
    return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool dir_lookup(const struct dir *dir, const char *name, struct inode **inode) {
    struct dir_entry e;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    if (strcmp(name, ".") == 0) {
        *inode = inode_open(dir->inode->sector);
    } else if (strcmp(name, "..") == 0) {
        *inode = inode_get_parent(dir->inode);
    } else if (lookup(dir, name, &e, NULL))
        *inode = inode_open(e.inode_sector);
    else
        *inode = NULL;

    return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool dir_add(struct dir *dir, const char *name, block_sector_t inode_sector) {
    struct dir_entry e;
    off_t ofs;
    bool success = false;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    /* Check NAME for validity. */
    if (*name == '\0' || strlen(name) > NAME_MAX) return false;

    /* Check that NAME is not in use. */
    if (lookup(dir, name, NULL, NULL)) goto done;

    /* Set OFS to offset of free slot.
       If there are no free slots, then it will be set to the
       current end-of-file.

       inode_read_at() will only return a short read at end of file.
       Otherwise, we'd need to verify that we didn't get a short
       read due to something intermittent such as low memory. */
    for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e; ofs += sizeof e)
        if (!e.in_use) break;

    /* Write slot. */
    e.in_use = true;
    strlcpy(e.name, name, sizeof e.name);
    e.inode_sector = inode_sector;
    success = inode_add_to_dir(inode_sector, dir->inode->sector) &&
              inode_write_at(dir->inode, &e, sizeof e, ofs) == sizeof e;

done:
    return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool dir_remove(struct dir *dir, const char *name) {
    struct dir_entry e;
    struct inode *inode = NULL;
    bool success = false;
    off_t ofs;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    /* Find directory entry. */
    if (!lookup(dir, name, &e, &ofs)) goto done;

    // /* See if trying to remove self */
    // if (strcmp(name, ".") == 0) {
    //     inode_close(dir->inode);
    //     dir->inode = NULL;
    // }

    /* Open inode. */
    inode = inode_open(e.inode_sector);
    if (inode == NULL) goto done;
    if (inode->data.is_directory) {
        if (inode->sector == thread_current()->current_directory) goto done;
        if (inode->data.directory_size != 0 || inode->open_cnt > 1) goto done;
    }

    /* Erase directory entry. */
    e.in_use = false;
    if (inode_write_at(dir->inode, &e, sizeof e, ofs) != sizeof e) goto done;

    /* Remove inode. */
    inode_remove(inode);
    inode_remove_from_dir(dir->inode->sector);
    success = true;

done:
    inode_close(inode);
    return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool dir_readdir(struct dir *dir, char name[NAME_MAX + 1]) {
    struct dir_entry e;

    while (inode_read_at(dir->inode, &e, sizeof e, dir->pos) == sizeof e) {
        dir->pos += sizeof e;
        if (e.in_use) {
            strlcpy(name, e.name, NAME_MAX + 1);
            return true;
        }
    }
    return false;
}

/* Changes the current directroy to dir_path */
bool dir_chdir(const char *dir_path) {
    char filename[NAME_MAX + 1];
    struct dir *dir = find_file_dir(dir_path, filename);
    struct inode *inode = NULL;

    bool success;
    if (strcmp(dir_path, "..") == 0) {
        success = dir != NULL;
        inode = inode_open(dir->inode->sector);
    } else {
        success = dir != NULL && dir_lookup(dir, filename, &inode);
    }
    dir_close(dir);

    if (success) {
        if (!inode->data.is_directory) {
            return false;
        }
        thread_current()->current_directory = inode->sector;
    }

    return success;
}

/* Returns the directory the file is stored in and stores name of
   the file in filename */
struct dir *find_file_dir(const char *full_path, char *filename) {
    if (full_path == NULL || strlen(full_path) == 0) {
        return NULL;
    }

    /* Decides whether to open an absolute path or relative path */
    struct inode *curr_inode;
    if (full_path[0] == '/') {
        curr_inode = inode_open(ROOT_DIR_SECTOR);
    } else {
        curr_inode = inode_open(thread_current()->current_directory);
    }
    struct dir *curr_directory = dir_open(curr_inode);

    if (strcmp(full_path, ".") == 0) {
        strlcpy(filename, full_path, NAME_MAX + 1);
        return curr_directory;
    }
    if (strcmp(full_path, "..") == 0) {
        struct inode *parent_inode = inode_get_parent(curr_inode);
        struct dir *parent_directory = dir_open(parent_inode);

        inode_close(curr_inode);
        dir_close(curr_directory);

        strlcpy(filename, full_path, NAME_MAX + 1);
        return parent_directory;
    }

    /* Walks through path until we reach the file */
    char prev_path_part[NAME_MAX + 1];
    char path_part[NAME_MAX + 1];
    int get_status = get_next_part(path_part, &full_path);
    while (get_status == 1) {
        struct inode *next_node;
        dir_lookup(curr_directory, path_part, &next_node);
        /* If we hit a file or can't go further, make sure it's the last part of the path */
        if (next_node == NULL || !next_node->data.is_directory) {
            inode_close(next_node);
            if (get_next_part(path_part, &full_path) == 0) {
                strlcpy(filename, path_part, NAME_MAX + 1);
                return curr_directory;
            } else {
                dir_close(curr_directory);
                return NULL;
            }
        }

        /* Close current directory and go to next directory */
        dir_close(curr_directory);
        inode_close(curr_inode);

        curr_inode = next_node;
        curr_directory = dir_open(next_node);

        strlcpy(prev_path_part, path_part, NAME_MAX + 1);
        get_status = get_next_part(path_part, &full_path);
    }

    /* If path part was too long then stop */
    if (get_status == -1) {
        return NULL;
    }

    if (curr_directory->inode->sector == 1) {
        strlcpy(filename, ".", NAME_MAX + 1);
        return curr_directory;
    }
    /* If last part of the path was a directory then have the filename reference itself */
    struct inode *parent_inode = inode_get_parent(curr_inode);
    struct dir *parent_directory = dir_open(parent_inode);

    dir_close(curr_directory);
    inode_close(curr_inode);

    strlcpy(filename, prev_path_part, NAME_MAX + 1);
    return parent_directory;
}

/* Extracts a file name part from *SRCP into PART, and updates *SRCP so that the
   next call will return the next file name part. Returns 1 if successful, 0 at
   end of string, -1 for a too-long file name part. */
static int get_next_part(char part[NAME_MAX + 1], const char **srcp) {
    const char *src = *srcp;
    char *dst = part;

    /* Skip leading slashes. If it’s all slashes, we’re done. */
    while (*src == '/') src++;
    if (*src == '\0') return 0;

    /* Copy up to NAME_MAX character from SRC to DST. Add null terminator. */
    while (*src != '/' && *src != '\0') {
        if (dst < part + NAME_MAX)
            *dst++ = *src;
        else
            return -1;
        src++;
    }
    *dst = '\0';

    /* Advance source pointer. */
    *srcp = src;
    return 1;
}