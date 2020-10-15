Final Report for Project 1: Threads
===================================

### Design Changes

The biggest difference in our final code came mainly in Part 2, our priority scheduler. 

For starters we realized that our priority bucket scheme to efficiently choose the next ready thread was an unneeded layer of complexity. While it was a great idea for writing an efficient solution in real world practice, we realized that the auto-grader didn't really care for runtime here. We thus changed our implementation to simply use the `list_max` function on our ready list to find the next thread. 

Perhaps the largest change in our design occurred when we found out a thread can hold multiple locks! Funnily enough using the printf command while debugging our lock scheme made us realize this. We were completely dismayed by the fluctuating results of our tests until we realized this. This led us to make a major revamp in our `thread struct`, `lock struct`, `lock_acquire()` and `lock_release()`. We began storing a list of all our `held_locks` instead of just one by adding a `list_elem` to the `lock_struct`. Next, and probably the biggest challenge, we changed `lock_release()`. To correctly update the thread's priority here, we had to come up with an algorithm to determine the priority of the max waiting thread out of all waiting threads for all our held locks. An algorithm, we learned, that was very easy to make a mistake in when working with several `list_elem` conversions. 

Finally some more missing details came to light in how we chained priority. We came up with a scheme to store both our `base_priority` , our original priority, and our "chained/donated" `priority`. We initially overlooked this as we forgot to account for special scenarios where set priority is called on our thread in the middle of some combination of donations. This also proved useful in `lock_release()` when the max waiter had a lower priority than our `base_priority`

In part 3 the only bump we came across was correctly setting the `ready_list` size to include/not include the `current_thread()`. Aside from this small change, our design for part 1 and 3 remained relatively consistent.

### Reflection

**What went well:** We started the project by carefully understanding almost every line in `thread.c & .h`, `timer.c & .h`, `sync.c & .h`. This was instrumental to our success in the project. Not only did it finally clear up a lot of misconceptions, it also introduced how to use many of the given resources such as initializing lists, storing and restoring interrupts,  and defining comparator functions. If anything this is at the same time something we could **improve on next time**. While we scanned these files for the design doc, we didn't go line by line understating exactly what was set up for us and how all of it interacted together. Doing this from the beginning for our next project will lead to a much better understanding and consequently a much better design doc. 

**Splitting up tasks:** We started by all walking  line by line of through these main files and our design ideas together so that everyone was on the same page when we began. We then tackled the project by splitting into groups of two. Two of us did the majority of Task 1 & 3 while the other focused on Task 2. We all took part in debugging each others tasks as it helped to get fresh eyes some times. We think this strategy of starting off together and then breaking off in pairs was pretty effective and something we will probably employ again.

