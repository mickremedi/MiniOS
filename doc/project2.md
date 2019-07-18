Design Document for Project 2: User Programs
============================================

## **Group Members**

- Ahmad Jawaid - *ahmadjawaid@berkeley.edu*
- Michael Remediakis - *m_remedi@berkeley.edu*
- Sarmad Farooq - *sarmadf@berkeley.edu*
- Joseph Zakher - *joseph.zakher@berkeley.edu*

# Task 1: Argument Passing

---

## Data Structures

    //These functions will be modified
    static void start_process(void *file_name_);
    struct list tokens;
    struct list tokenAddresses;

## Algorithm

Edit `static void start_process(void *file_name_)` to support command line arguments by creating `argc` and `argv`:

1. Beginning after the `load()` call, split `file_name` into separate tokens using `strtok` where the separator is a space
2. Create two list, `tokens` and `tokenAddresses`
3. Push each result from strtok to the front of tokens sequentially so that they will be ordered in reverse and at the same time, push the equivalent address that the argument will be saved relative to the esp
4. Store the number of arguments added
5. Iterate through the list
    1. Use `memcopy()` to copy the value of an item to the address `if_.esp` is pointing to, then decrement `if_.esp` by the amount of bytes copied.
6. If the resulting address of `if_.esp` is not divisible by 4, use the mod operation to find out how many remaining bytes are left to add, then push 0 to the stack as well
7. Use memcopy() to push 0 to the stack as the null argument of argv
8. Iterate through `tokenAddresses` and push each address to the stack
9. Push the current address of the esp to the stack to save the pointer to argv
10. Push the number of arguments to the stack
11. Push 0 to the stack to represent a null return address

## Synchronization

Since `start_process` has to run independent of the scheduler, there shouldn't be any synchronization errors.

## Rationale

We decided to use the list struct because it allowed for more dynamic allocation of the list. It also made the allocation onto the stack easier because it easily allowed us to insert the arguments in the correct order.

# Task 2: Process Control Syscall

---

## Data Structures

    

## Algorithm & Synchronization

1. Implement the ability to make syscalls
    1. Need to find a way to safely read and write memory in the user process's virtual address space.
    2. Since the syscall functionality is built into `syscall_handler()`, all implementations of syscalls will be put into this function. We will check for what logic to apply by checking `args[0]` and comparing it to its corresponding enum in syscall-nr.h. 
2. Update Pintos to support the following syscalls:
    - [ ]  `int practice(int i)`
        1. if `args[0] == SYS_PRACTICE`
        2. Take the integer that is in args[1], increments it, and returns the result stored in the `eax` register.
    - [ ]  `void halt(void)`
        1. if `args[0] == SYS_HALT`
        2. Shut down pintos by calling `shutdown_power_off()`
    - [ ]  `void exit (int status)` **already implemented**
        1. Terminates the current user program
        2. Returns the status of the kernel (typically 0 for success and nonzero for error)
    - [ ]  `pid_t exec(const char *cmd_line)`
        1. if `args[0] == SYS_EXEC` 
        2. Call `process_execute(const char *file_name)`, passing in `args[1]` as the argument. 
        3. We will call sema down right after this  
        4. Each thread will hold its semaphore which we will increment once the thread has been created
        5. The process_execute function return a `tid_t` which we will use as our `pid_t` . We can check this to see whether there was an error during loading of the program. 

    - [ ]  `int wait(pid_t pid)` Make tests for this one have it use `process_wait()` **— Needs to edit**
        1. Wait for the child processed identified by `pid` to exit.
        2. Retrieves the child's exit status
        3. If the child was terminated by the kernel (exception), set the exit status to -1
        4. Also set the status to -1 if:
            1. pid is not a direct child of the caller
            2. The caller has already called wait on the same pid
        5. Return the status

# Task 3: File Operation Syscall

---

## Data Structures

    /* Global lock for input and output handling because 
    pintos is only allowed to read and write from files */
    struct lock io_lock;

## Overview of Task

1. Use the appropriate functions in the file system library to implement the file syscalls
2. Make sure that syscalls don't call multiple filesystem operations concurrently, so use global locks on the filesystem operations.
3. When a user process is running, no one is able to modify the executable
    1. Use `file_deny_write()` and `file_allow_write()`
4. Create the following file syscalls, where we aquire `io_lock` before any of the calls and release it after:
    - [ ]  `bool create(const char *file, unsigned initial_size)`
        1. Create a file called `file` of size `initial_size` using `filesys_create()` and store the result
        2. Return the result which is true if successful and false otherwise
    - [ ]  `bool remove(const char *file)`
        1. Delete the file called `file` using `filesys_remove()` regardless of whether it is open or closed (removing does not close the file)
        2. Return true if successful, otherwise false
    - [ ]  `int open(const char *file)`
        1. Opens the file called `file`
        2. Retrieve the file's file descriptor, a nonnegative integer
            1. Even for the same file, the file descriptor is different for every process and they each have to be closed independently
        3. Return the file descriptor or -1 if the file couldn't be opened
            1. Not allowed to return 0 or 1, those are reserved for stdin and stdout respectively
    - [ ]  `int filesize(int fd)`
        1. Returns the size of the file referenced by its file descriptor, `fd` using `file_length`
    - [ ]  `int read(int fd, void *buffer, unsigned size)`
        1. Reads `size` bytes from the file referenced by `fd` using `file_read`
            1. If `fd` is 0, use `input_getc()`
            2. If at end of file read 0 bytes
        2. Store the read bytes in `buffer`
        3. Return the number of bytes read or -1 if the file couldn't be read
    - [ ]  `int write(int fd, const void *buffer, unsigned size)`
        1. Write `size` bytes from `buffer` to file referenced by `fd` using `file_write`
            1. if `fd` is 1, use `putbuf()`
        2. Stop writing once end-of-file is reached because file growth isn't implemented
        3. Return the number of bytes written or -1 if the file couldn't be written to
    - [ ]  `void seek(int fd, unsigned position)`
        1. Change the next byte that would be read or written in `fd` using `file_seek`
        2. `position` represents the number of bytes from the beginning of the file, where 0 is at the beginning
        3. If position moves past the end of the file, an error will occur (already implemented by file system)
    - [ ]  `unsigned tell(int fd)`
        1. Get the position of the next byte to be read or written in `fd` using `file_tell`
        2. Return this position expressed in bytes from the beginning of the file
    - [ ]  `void close(int fd)`
        1. Close the file referenced by `fd` using `file_close`
            - Exiting or terminating a process should close all open file descriptors

## Synchronization

To prevent synchronization issues, we included a global lock to the syscalls which would block multiple file syscalls to be used.

# Additional Questions

1. **Sc-bad-sp.c:** In line 18 of the the test file we see the `%esp` being incremented by a large number of bytes essentially setting it to an invalid address. This should be handled safely by our implementation and thus the test expects us to return with `exit(-1)`
2. **Sc-bad-arg.c:** At line 14 we see the test file do two things, set both `SYS_EXIT` and  the `%esp` to the top of the stack. Again this will lead unavailable user process memory because of the arguments and should again make us return  with `exit(-1)`

### GDB Questions

1. Main at address 0xc0007d50. Other threads: idle, *(pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067})*
2. (gdb) backtrace

#0 process_execute (file_name=file_name@entry=0xc0007d50 "args-none") at ../../userprog/process.c:32

#1 0xc002025e in run_task (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:288

    /* Runs the task specified in ARGV[1]. */
    static void
    run_task (char **argv)
    {
      const char *task = argv[1];
    
      printf ("Executing '%s':\n", task);
    #ifdef USERPROG
      process_wait (process_execute (task));
    #else
      run_test (task);
    #endif
      printf ("Execution of '%s' complete.\n", task);
    }

#2 0xc00208e4 in run_actions (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:340

    /* Executes all of the actions specified in ARGV[]
       up to the null pointer sentinel. */
    static void
    run_actions (char **argv)
    {
      /* An action. */
      struct action
        {
          char *name;                       /* Action name. */
          int argc;                         /* # of args, including action name. */
          void (*function) (char **argv);   /* Function to execute action. */
        };

#3 main () at ../../threads/init.c:133

3. args-none\000\000\000\000\000\000 at 0xc0109000. The other present threads are main and idle.

4. 

    tid = thread_create(file_name, PRI_DEFAULT, start_process, fn_copy);

5. 0x0804870c

6. #0 _start (argc=<error reading variable: can't compute CFA for this frame>, argv=<error reading variable: can't compute CFA for this frame>) at ../../lib/user/entry.c:9

7. When trying to access memory, the latter wasn't mapped into our virtual address space.
