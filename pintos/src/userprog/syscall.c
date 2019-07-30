#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "kernel/list.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/input.h"

struct lock io_lock;
static struct list file_list;
int global_fd;

struct file_info {
    tid_t owner;
    int fd;
    struct file *file;  // this holds the file
    struct list_elem elem;
    const char *file_name;  // this is the name of the file
};

static void syscall_handler(struct intr_frame *);
struct file_info *get_file(int fd);
int add_fd(struct file *file, const char *file_name);
bool valid_pointer(void *ptr, size_t size);

struct file_info *get_file(int fd) {
    struct list_elem *e;
    for (e = list_begin(&file_list); e != list_end(&file_list);
         e = list_next(e)) {
        struct file_info *f = list_entry(e, struct file_info, elem);
        if (f->fd == fd && f->owner == thread_current()->tid) {
            return f;
        }
    }
    return NULL;
}

int add_fd(struct file *file, const char *file_name) {
    struct file_info *file_node = malloc(sizeof(struct file_info));
    file_node->file = file;
    file_node->file_name = file_name;
    file_node->fd = global_fd++;
    file_node->owner = thread_current()->tid;
    list_push_back(&file_list, &file_node->elem);
    return file_node->fd;
}

void close_all_files(tid_t tid) {
    struct list_elem *e = list_begin(&file_list);
    while (e != list_end(&file_list)) {
        struct file_info *f = list_entry(e, struct file_info, elem);
        e = list_next(e);
        if (f->owner == tid) {
            file_close(f->file);
            list_remove(&f->elem);
            free(f);
        }
    }
}

bool valid_pointer(void *ptr, size_t size) {
    if (!is_user_vaddr(ptr) ||
        pagedir_get_page(thread_current()->pagedir, ptr) == NULL) {
        return false;
    }
    if (!is_user_vaddr(ptr + size) ||
        pagedir_get_page(thread_current()->pagedir, ptr + size) == NULL) {
        return false;
    }
    return true;
}

void syscall_init(void) {
    global_fd = 2;
    list_init(&file_list);
    lock_init(&io_lock);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f UNUSED) {
    uint32_t *args = ((uint32_t *)f->esp);

    if (!valid_pointer(args, sizeof(uint32_t))) {
        thread_exit();
    }

    /* -----------PROCESS SYSCALLS----------- */

    /* int practice(int i) */
    if (args[0] == SYS_PRACTICE) {
        int i = (int)args[1];
        f->eax = i + 1;
        return;
    }

    /* void halt(void) */
    if (args[0] == SYS_HALT) {
        shutdown_power_off();
        NOT_REACHED();
    }

    /* void exit (int status)  */
    if (args[0] == SYS_EXIT) {
        thread_current()->babysitter->exit_code = args[1];
        f->eax = args[1];
        thread_exit();
    }

    /* pid_t exec(const char *cmd_line) */
    if (args[0] == SYS_EXEC) {
        const char *file_name = (const char *)args[1];
        if (!valid_pointer((void *)file_name, sizeof(char *))) {
            thread_exit();
        }
        f->eax = process_execute(file_name);
        return;
    }

    /* int wait(pid_t pid) */
    if (args[0] == SYS_WAIT) {
        tid_t pid = (tid_t)args[1];
        f->eax = process_wait(pid);
        return;
    }

    /* -----------FILE SYSCALLS----------- */

    /* bool create(const char *file, unsigned initial_size) */
    if (args[0] == SYS_CREATE) {
        lock_acquire(&io_lock);
        const char *file_name = (const char *)args[1];
        unsigned size = (unsigned)args[2];
        if (!valid_pointer((void *)file_name, sizeof(char *))) {
            thread_exit();
        }
        f->eax = filesys_create(file_name, size);
        lock_release(&io_lock);
        return;
    }

    /* bool remove(const char *file) */
    if (args[0] == SYS_REMOVE) {
        lock_acquire(&io_lock);
        const char *file_name = (const char *)args[1];
        if (!valid_pointer((void *)file_name, sizeof(char *))) {
            thread_exit();
        }
        f->eax = filesys_remove(file_name);
        lock_release(&io_lock);
        return;
    }

    /* int open(const char *file) */
    if (args[0] == SYS_OPEN) {
        // lock_acquire(&io_lock);

        const char *file_name = (const char *)args[1];
        if (!valid_pointer((void *)file_name, sizeof(char *))) {
            thread_exit();
        }

        struct file *file = filesys_open(file_name);
        if (file == NULL) {
            f->eax = -1;
        } else {
            f->eax = add_fd(file, file_name);
        }

        // lock_release(&io_lock);
        return;
    }

    /* Write to standard output */
    if (args[0] == SYS_WRITE) {
        lock_acquire(&io_lock);
        int fd = (int)args[1];
        char *buffer = (char *)args[2];
        if (!valid_pointer(buffer, sizeof(char *))) {
            thread_exit();
        }
        int size = (int)args[3];
        if (fd == 1) {
            while (size >= 100) {
                putbuf(buffer, 100);
                buffer += 100;
                size -= 100;
            }
            putbuf(buffer, size);
            f->eax = (int)args[3];
            lock_release(&io_lock);
            return;
        }
        lock_release(&io_lock);
    }

    /* Read from standard input */
    if (args[0] == SYS_READ) {
        lock_acquire(&io_lock);
        int fd = (int)args[1];
        uint8_t *buffer = (uint8_t *)args[2];
        if (!valid_pointer(buffer, sizeof(char *))) {
            thread_exit();
        }
        unsigned size = (unsigned)args[3]; 
        if(fd == 0) {
            size_t count = 0;
            while( count < size ){
                buffer[count] = input_getc();
                if (buffer[count] == '\n'){
                    break;
                }
                count++;
            }
            f->eax = count;
            lock_release(&io_lock);
            return;
        }
        lock_release(&io_lock);
    }

    lock_acquire(&io_lock);
    struct file_info *file_node = get_file(args[1]);
    lock_release(&io_lock);

    if (file_node == NULL) {
        f->eax = -1;
        return;
    }

    /* int filesize(int fd) */
    if (args[0] == SYS_FILESIZE) {
        lock_acquire(&io_lock);
        f->eax = file_length(file_node->file);
        lock_release(&io_lock);
        return;
    }

    /* int read(int fd, void *buffer, unsigned size) */
    if (args[0] == SYS_READ) {
        lock_acquire(&io_lock);
        void *buffer = (void *)args[2];
        if (!valid_pointer(buffer, sizeof(char *))) {
            thread_exit();
        }
        unsigned size = (unsigned)args[3];
        f->eax = file_read(file_node->file, buffer, size);
        lock_release(&io_lock);
        return;
    }

    /* int write(int fd, const void *buffer, unsigned size) */
    if (args[0] == SYS_WRITE) {
        lock_acquire(&io_lock);
        void *buffer = (void *)args[2];
        if (!valid_pointer(buffer, sizeof(char *))) {
            thread_exit();
        }
        unsigned size = (unsigned)args[3];
        f->eax = file_write(file_node->file, buffer, size);
        lock_release(&io_lock);
        return;
    }

    /* void seek(int fd, unsigned position) */
    if (args[0] == SYS_SEEK) {
        lock_acquire(&io_lock);
        file_seek(file_node->file, args[2]);
        lock_release(&io_lock);
        return;
    }

    /* unsigned tell(int fd) */
    if (args[0] == SYS_TELL) {
        lock_acquire(&io_lock);
        f->eax = file_tell(file_node->file);
        lock_release(&io_lock);
        return;
    }

    /* void close(int fd) */
    if (args[0] == SYS_CLOSE) {
        lock_acquire(&io_lock);
        file_close(file_node->file);
        list_remove(&file_node->elem);
        free(file_node);
        lock_release(&io_lock);
        return;
    }
}
