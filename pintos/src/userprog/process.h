#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define ARG_LIM 64

tid_t process_execute(const char *file_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(void);

struct babysitter {
    struct list_elem child_elem;   /* Element to store ourselves in our parent
                                      threads' children list */
    struct semaphore sema_loading; /* Semaphore used to track thread creation */
    tid_t tid;                     /* Thread identifier. */
    int exit_code;
    bool load_success;
};

#endif /* userprog/process.h */
