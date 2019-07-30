# Final Report

### Design Changes

Our biggest changes came in part 2. For `exec` and `wait` , our initial design proposed  having all our children threads notify a `semaphore` stored in our thread. Due to insight from our TA in the design review we realized this was prone to synchronization errors. If all our children tried updating their parent thread it could cause several race conditions. Moreover it would take a lot more hard work and overhead to get it right. Thus we realized its a lot better to store this `semaphore` inside each child thread. Hence we added all necessary variables for these commands in our `thread.h` struct and initialized them whenever we forked a new child. We proceeded to `wait` on the newly created semaphore in our baby thread. This solution was nearly perfect until we came to writing our `exit` function. 

As we wrote the `exit` algorithm to close our files and delete our children we realized we had lost all memory access to them when our thread was destroyed. This was particularly dismaying as we found ourselves in a catch 22 of sorts; we couldn't do one without the other! We were halted here for a while until we finally came up with an ingenious solution. Instead of storing all the appropriate variables in `thread.h` we created a new `babysitter` struct, an apt name for helping our child thread get on its feet,  in `process.h` and malloced space for it. This technique let us store the variables in an independent portion of the memory as our thread would only store the pointer to its `babysitter`. This time around when a thread was destroyed the `babysitter` struct and the variables we needed weren't deleted. The only challenge left was to now somehow recover this pointer. We achieved this by having a `list elem` in the `babysitter` and making our parent thread store a list of all their `babysitters` instead of all their `children`. While this is not recommended for actual life, it works out well for operating systems. We now were able to successfully recover variables we needed to close our file and children upon exiting. The last thing we had to be careful off was freeing all the `babysitter` malloced space.

### Reflection

**What went well:** This time around the project was a lot easier to tackle as we had grown accustomed to pintOS and become familiar with how many of it operations worked (lists, interrupts etc). Furthermore we employed our learnings of last time to spend time understanding new files and functions given to us. By doing that in the beginning we were a lot quicker on making progress and understanding what was needed of us. One thing we could **improve on** still would be the thoroughness of our design doc. While we got it all done we should maybe analyze it more considering pitfalls and loopholes. This would help us save time immensely by reducing design refactoring upon hitting one of the pitfalls. 

**Splitting up tasks:** Our split was similar to before, we all started by walking through the main files and our design ideas together so that everyone was on the same page. Due to the effectiveness from before, we then tackled the project by splitting into groups of two. This time we all tackled part 1 together - Michael drove to design and coding here but we all participated and understood. We then broke off with one team working on part 2 and the other team working on part 3. When someone was done their task they began helping others. Michael Remediakis deserves a huge shout out here for helping out everyone alongside his contributions.

---

# Student Testing Report

## read-stdin

Our first test involved reading from the standard input. This is a really simple test, but it hadn't seemed to have been checked.

To create a test for this we would start by writing in the main a simple read with the file descriptor as 0. Doing so would have the program wait until an input is received from the keyboard. We would then check to see if the correct number of bytes was read as requested. However, we were unable to find a way to redirect the standard input so that pintos would be able to detect a redirection from a given file to the standard input. One possible way we had tried to do this was to utilize the dup2 syscall however, since it is supposed to be run in pintos, we hadn't created an implementation of dup or dup2, so there was no way for us to access it.

However here is the test code:

    #include <stdio.h>
    #include <syscall.h>
    #include "tests/main.h"
    
    void
    test_main (void)
    {
        int byte_cnt;
        char buf;
        
        byte_cnt = read (STDIN_FILENO, &buf, 1);
        
        if (byte_cnt != 1)
            fail ("read() returned %d instead of 1", byte_cnt);
            
    }