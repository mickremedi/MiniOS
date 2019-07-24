#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler(struct intr_frame*);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame* f UNUSED) {
    uint32_t* args = ((uint32_t*)f->esp);
    printf("System call number: %d\n", args[0]);
    if (args[0] == SYS_EXIT) {
        f->eax = args[1];
        char* thread_name = thread_current()->name;
        printf("%s: exit(%d)\n", thread_name, args[1]);
        thread_exit();
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
