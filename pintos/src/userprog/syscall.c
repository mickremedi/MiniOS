#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/inode.h"
#include "kernel/list.h"


struct lock io_lock;
static struct list file_list;
int global_fd;

struct file_info{
    int fd;
    struct file *file_name; //this holds the file 
    struct list_elem elem;
    const char* file; //this is the name of the file
};

void set_file(struct file_info *file_node, struct file *file){
    file_node->file_name = file;
}

struct file_node * get_file(int fd){
    struct list_elem *e;
    for (e = list_begin (&file_list); e != list_end (&file_list);
           e = list_next (e))
        {
          struct file_info *f = list_entry (e, struct file_info, elem);
          if (f->fd == fd){
              return f->file_name;
          }
        }
    return NULL;
}

bool file_exists(const char* file_name){
    struct list_elem *e;
    for (e = list_begin (&file_list); e != list_end (&file_list);
           e = list_next (e))
        {
          struct file_info *f = list_entry (e, struct file_info, elem);
          if (strcmp(f->file_name,file_name) == 0){
              return true;
          }
        }
    return false;
}

void set_fd(struct file_info *file_node, int fd){
    file_node->fd = fd;
}

int get_fd(struct file_info *file_node){
    return file_node->fd;
}

int push_to_file_list(struct file *file, const char* file_name ){
    struct file_info *file_node = malloc(sizeof(struct file_info));
    set_file(file_node,file);
    file_node->file_name = file_name;
    global_fd++;
    set_fd(file_node,global_fd);
    list_push_back(&file_list, &file_node->elem);
    return get_fd(file_node);
}

static void syscall_handler(struct intr_frame*);

bool valid_pointer(void* ptr, size_t size) {
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
    global_fd = 1;
    list_init(&file_list);
    lock_init(&io_lock);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame* f UNUSED) {
    uint32_t* args = ((uint32_t*)f->esp);

    if (!valid_pointer(args, sizeof(uint32_t))) {
        thread_exit();
    }

    /* Checks that esp in user space */
    if (args[0] == SYS_EXIT) {
        thread_current()->babysitter->exit_code = args[1];

        f->eax = args[1];
        thread_exit();
    }

    if (args[0] == SYS_EXEC) {
        const char* file_name = (const char*)args[1];
        if (!valid_pointer(file_name, sizeof(char*))) {
            thread_exit();
        }
        f->eax = process_execute(file_name);
    }

    if (args[0] == SYS_WAIT) {
        tid_t pid = (tid_t)args[1];
        f->eax = process_wait(pid);
    }

    if (args[0] == SYS_CREATE) {
        lock_acquire(&io_lock);
        const char* file_name = (const char*)args[1];
        unsigned size = (unsigned)args[2];
        if (!valid_pointer(file_name, sizeof(char*))) {
            thread_exit();
        }
        f->eax = filesys_create(file_name, size);
        lock_release(&io_lock);
    }

    if (args[0] == SYS_REMOVE) {
        lock_acquire(&io_lock);
        const char* file_name = (const char*)args[1];
        if (!valid_pointer(file_name, sizeof(char*))) {
            thread_exit();
        }
        f->eax = filesys_remove(file_name);
        lock_release(&io_lock);
    }

    if (args[0] == SYS_WRITE) {
        lock_acquire(&io_lock);
        int fd = (int)args[1];
        char* buffer = (char*)args[2];
        if (!valid_pointer(buffer, sizeof(char*))) {
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
        } else {
            // f->eax = file_write(fd, buffer, size);
        }

        lock_release(&io_lock);
    }

    if (args[0] == SYS_PRACTICE) {
        int i = (int)args[1];
        f->eax = i + 1;
        return;
    }

    if (args[0] == SYS_HALT) {
        shutdown_power_off();
        NOT_REACHED();
    }

    if (args[0] == SYS_OPEN) {
        lock_acquire(&io_lock);
        struct file *file;
        const char* file_name = (const char*)args[1];
        if (file_exists(file_name)){
            file = file_reopen(file_name);
        }
        else{
            file = filesys_open(file_name);
        }
        
        if (file == NULL){
            f->eax = -1;
        }
        // struct inode *file_node = file_get_inode (file);
        // f->eax = inode_get_inumber(file_node);
        f->eax = push_to_file_list(file, file_name);
        lock_release(&io_lock);
    }

    if (args[0] == SYS_FILESIZE) {
        lock_acquire(&io_lock);
        struct file_info *file_node = get_file(args[1]);
        struct file *file = file_node->file_name;
        if (file == NULL){
            f->eax = 0;
        }
        f->eax = file_length(file); 
        lock_release(&io_lock);
    }

    if (args[0] == SYS_READ) {
        lock_acquire(&io_lock);
        struct file_info *file_node = get_file(args[1]);
        struct file *file = file_node->file_name;
        f->eax = file_read(file,args[2],args[3]);
        lock_release(&io_lock);
    }
    
    if (args[0] == SYS_SEEK) {
        lock_acquire(&io_lock);
        struct file_info *file_node = get_file(args[1]);
        struct file *file = file_node->file_name;
        file_seek(file, args[2]);
        lock_release(&io_lock);

    }

    if (args[0] == SYS_TELL) {
        lock_acquire(&io_lock);
        struct file_info *file_node = get_file(args[1]);
        struct file *file = file_node->file_name;
        f->eax = file_tell(file);
        lock_release(&io_lock);
    }

    if (args[0] == SYS_CLOSE) {
        lock_acquire(&io_lock);
        struct file_info *file_node = get_file(args[1]);
        struct file *file = file_node->file_name;
        file_close(file);
        // if (global_fd > 1){
        //     global_fd--;
        // }
        list_remove(&file_node->elem);
        lock_release(&io_lock);
    }
}
