# Project 1: Threads - Design Doc

## **Group Members**

- Michael Remediakis - *m_remedi@berkeley.edu*
- Ahmad Jawaid - *ahmadjawaid@berkeley.edu*
- Joseph Zakher - *joseph.zakher@berkeley.edu*
- Sarmad Farooq - *sarmadf@berkeley.edu*

# Task 1: Efficient Alarm Clock

## Data structures and functions

**Modification of the struct thread in 'thread.h':**

    /* To each thread add a variable to store the number of ticks it should sleep*/
    int64_t ticks_sleep; 

**Modification in 'timer.c':**

    /*
    This will be our sleeping thread queue sorted based in ascending
    order of the thread's ticks_asleep
    */
    static struct list nap_room; 
    

## Algorithms

In `timer_sleep(int64_t ticks)`:

1. Figure out who the calling thread 
2. Put the thread in a suspended mode until `ticks` time has passed
    1. Set the thread's `ticks_sleep` to be the current global ticks + passed in ticks
    2. Disable interrupts for synchronization and do:
        1. Insert into our `nap_room` list in sorted order
        2. Block the thread 
    3. Enable interrupts

In  `timer_interrupt()`:

1. We increment the global ticks
2. Check the head of `nap_room` and if the threads sleep time is equivalent to global ticks we remove it from our list. 
    1. Disable interrupts for synchronization and do:
        1. Remove thread from `nap_room` 
        2. Unblock thread
    2. Enable interrupts
3. Then we call `thread_tick()`

## Synchronization

**Potential concurrent accesses to shared resources**

Since lists are a common cause for synchronization issues, we determined two potential areas where concurrent calls could be made to our `nap_room`. Once in `timer_sleep` and once in `timer_interrupt`. In both cases we dealt with the issue by temporarily disabling interrupts. 

## Rationale

This is a simple and easy to run design. By keeping the `nap_room` ordered we have  a thread insertion time of O(n); however, we save time at every tick in the interrupt handler by comparing the head of the list and removing it if needed. The time complexity for this check and remover operation will be O(1)! 

# Task 2: Priority Scheduler

## Data Structures and functions

### **Modification in 'thread.h'**

**Scheduling**

    /*
    Finding out which thread to schedule next requires us to take into account the 
    priority of the thread, so these functions would need to be modified to take 
    this into account.
    */
    static struct thread *next_thread_to_run (void);
    
    /* This will be a circular doubly linked list to represent our priority queue. */
    struct list priority_bucket_list;
    
    /* This represent's each bucket, it will contain a node'd with same priority. */
    struct list priority_bucket;
    
    /* This represent's the nodes in the buckets. It points to its respective thread. */ 
    struct list_elem priority_node;

**Locks**

    /* The thread struct needs to be modified to be able to hold locks */
    struct lock* held_lock;

**Priority Donations**

    /* Priority donations begin when a lock needs to be accessed that is currently 
    being accessed by another thread. So the lock is saved in a variable to test 
    until it is released. */
    struct lock* lost_lock;
    
    /* The original priority of the thread is saved for when the lock is released 
    and priority can return to the donor. */
    int original_priority;
    int effective_priority;
    
    /* The locks will be given a list a threads that are waiting for the lock to 
    open */
    static list waitlist;
    
    /* These functions will need to be modified to accommodate these new 
    variables. */
    static struct thread *next_thread_to_run (void);
    static void init_thread (struct thread *, const char *name, int priority);
    
    
    void thread_set_priority(int new_priority);
    int thread_get_priority(void);

## Algorithms

**What we need:**

1. Modify scheduler so that higher priority threads are run before lower priority threads
2. Modify locks, semaphores, and condition variables to prefer higher priority threads over lower priority threads
3. Implement Priority Donation

**Choosing the next thread to run**

1. Edit `next_thread_to_run()` to find the thread with the maximum priority from the `priority_bucket_list`.
2. Once found, use `list_remove()` to remove the thread from the `ready_list`

**Acquiring a Lock**

1. Since locks are dependent on semaphores, we'll first edit `sema_up()` to consider priorities which would then chain to cause locks to consider priorities too.
2. When a thread tries to acquire a lock, one of two things will happen:
    1. The thread will be successful and will now store the lock in `held_lock`.
    2. The thread is unsuccessful and needs to wait for the lock to close. 
        1. In order to do that, the  thread places itself into the waitlist for the lock.
        2. If the lock's owner has a lower priority than the current thread, the current thread changes the priority of the lock's owner to the priority of the current thread
        3. If the lock's owner is also waiting for another, lower priority, lock, then the owner of that lock will also receive the priority of the current thread.
        4. We will continue this iteration until we reach a thread that isn't waiting on anyone.

**Releasing a Lock**

1. When it's time for a lock to be released, it will give ownership to the thread with the highest priority and unblock the thread. And since that thread should have at most the priority of the lock, no priority donation needs to be done.
2. If the priority of the holding thread was donated, it will revert back to it's original priority.

**Computing the effective priority**

The effective priority of a thread will be held in the  `effective_priority` variable and will always be equal to the priority of it's donor or it's original priority.

**Priority scheduling for condition variables and locks**

Since monitors and locks depend on semaphores, modifying semaphores will modify the locks and monitors as well.

**Changing thread’s priority**

When a thread changes priority, it is moved to the bucket of the equivalent priority.

## Synchronization

**Potential concurrent accesses to shared resources**

We solve synchronization issues by using locks on the scheduler when updating priority. We determined that the biggest potential for concurrent accesses was if multiple threads were to attempt to update their priority. The scheduler could easily run into a race conditions so the best way to resolve it would be temporarily put a lock during an update.

## Rationale

- We chose a linked list of buckets instead of just a sorted queue because it would take a constant amount of time to find the correct bucket and to insert into the bucket.
- Another advantage of the bucket design is that it makes it simple to change priority. We can immediately take the thread node out of its bucket and reallocate it to the new bucket.
- Furthermore, the bucket design makes it easy to round robin schedule the threads with the same priority within the same bucket.
- Finally, we gave the lock a waitlist so that it know's which threads it can unlock. We always choose the thread with the highest priority..

# Task 3: Multi-level Feedback Queue Scheduler (MLFQS)

## Data structures and functions

    int thread_get_nice(void) // get the thread nice value
    void thread_set_nice(int new_nice) //  set the thread value
    int thread_get_recent_cpu(void) //gets threads recent cpu
    int thread_get_load_average(void) // returns load average 
    
    fixed_point_t load_avg; 
    
    struct thread{
    	int nice;
    	fixed_pont_t recent_cpu; 
    };
    

## Algorithms

For this algorithm we used the approach for the MLFQs scheduler provided to us in additional questions 

1. Increment the threads recent_cpu value by 1 for each tick 
2. Compute threads priority at every 4 ticks 
3. Compute all threads new priorities and redistribute into  appropriate buckets when ticks is divisible by freq
4. Increment through list and do:
    1. Compute thread new priority and reallocate 
    2. Keep track of max priority 

## Synchronization

We can prevent any concurrent calls by doing this logic inside our interrupt handler. This way we guarantee no two accesses being made at the same time.

## Rationale

As provided in the additional question, this scheduler allows us to offer a certain level of niceness to our scheduler that keeps certain threads from starving out other threads with lower natural priorities.

# Additional Questions

## Question 1

**Test:**

Initialize three threads, T1 with priority 10, T2 with priority 20, T3 with priority 30, and T4 with priority 30. Allow the scheduler to begin, where it would select T3 to go first. We then initialize a semaphore, S, to 0. We have T4 immediately call `sema_down()` on  S which puts T4 to sleep. We do the same with T3. T2 gets activated next and acquires a lock, L, and then calls `sema_down()` on S as well, putting T2 to sleep as well. T1 calls `sema_up()` on S, then yields to the scheduler. T4 then opens back up and tries to acquire L, but can't so it donates it's priority to T2. The scheduler returns to T1 which calls `sema_up()` again and yields to the scheduler again. This causes T3 to open up and print "Hello" then return. Then T2 opens and  prints "World" and returns.

**Problem:**

The problem that happened in the test was that the semaphore changes to T3 first at the end because  it has a higher base priority. In reality, it should be T2 that opened up and print "World" before it finishes and moves to T3 to print "Hello".

Expected Output:

Prints "World" then "Hello"

Actual Output:

Prints "Hello" then "World"

## Question 2:

let 1 second = 100 ticks:

priority = PRI_MAX - ( recent_cpu/4) - (nice x 2)

recent_cpu = 1 per tick thread ran and updated each 100 ticks by (2 x load_avg)/(2 x load_avg + 1 ) x recent_cpu + nice

load_avg = (59/60) x load_avg + (1/60) x ready_threads

[Untitled](https://www.notion.so/a9a0514667864a229d6aab4e086c6eba)

## Question 3:

The uncertainty arises when when there is a similar priority value. We apply round robin in that case to the list of ready threads. In case of two similar priorities and only one exist in ready threads, we chose the thread in the ready threads. If both are in ready threads we pick the first element in list of ready threads and run the one with the lowest run time.