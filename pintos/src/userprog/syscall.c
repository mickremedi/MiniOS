#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"


struct lock io_lock;
static void syscall_handler(struct intr_frame*);

void syscall_init(void) {
    lock_init(&io_lock);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame* f UNUSED) {
    uint32_t* args = ((uint32_t*)f->esp);
    // printf("System call number: %d\n", args[0]);
    if (args[0] == SYS_EXIT) {
        f->eax = args[1];
        char* thread_name = thread_current()->name;
        printf("%s: exit(%d)\n", thread_name, args[1]);
        thread_exit();
    }
    
    if(args[0] == SYS_CREATE){
        lock_acquire(&io_lock);
        const char* file_name = (const char *) args[1];
        unsigned size = (unsigned) args[2];
        f->eax = filesys_create(file_name, size); 
        lock_release(&io_lock);
    }
    
    if(args[0] == SYS_REMOVE){
        lock_acquire(&io_lock);
        const char* file_name = (const char*) args[1];
        f->eax = filesys_remove(file_name);
        lock_release(&io_lock);
    }

    if(args[0] == SYS_WRITE){
        lock_acquire(&io_lock);
        int fd = (int) args[1];
        char* buffer = (char *) args[2];
        int size = (int) args[3];
        if (fd == 1){
            while(size >= 100){
                putbuf(buffer,100);
                buffer += 100;
                size -= 100;
            }
            putbuf(buffer, size);
            f-> eax = (int) args[3];
        }
        else{
            // f->eax = file_write(fd, buffer, size);
        }
    
        lock_release(&io_lock);
        
    }

    // if (args[0] == SYS_PRACTICE) {
    //     char* endptr;
    //     int i = strtol(args[1], endptr, 10);
    //     f->eax = i;
    //     return;
    // }

    // if (args[0] == SYS_HALT) {
    //     shutdown_power_off();
    //     NOT_REACHED();
    // }

    // if (args[0] == SYS_EXEC) {
    //     char* file_name = args[1];
    // }

    // if (args[0] == SYS_WAIT) {
    // }
}
