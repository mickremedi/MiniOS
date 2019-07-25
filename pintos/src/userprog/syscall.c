#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

struct lock io_lock;
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
}
