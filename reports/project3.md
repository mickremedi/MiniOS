Final Report for Project 3: File System
=======================================

### Design Changes

As we embarked on part 2 we realized our design was missing many details and could be tackled better. For starters we recognized the the `free_map_allocate` function only attempted to allocate contiguous blocks of memory, our solution here was to change the function to be able to allocate non-contiguous blocks of memory. After discssing this in the design review we realized this was unneccsarry and would lead to changing a lot more things further down the road. Instead we came up with a wrapper function, `allocate_nonconsec`, that takes in how many blocks (N) we want and proceeds to call `free_map_allocate` N times. We also had to come up with another function, `free_map_available`, that would first check to make sure none of those N calls would fail. This helped prevent having to iterate backwards in `allocate_nonconsec` incase we ran out of space in the middle of the N calls. 

Our intial design doc also didn't put in enough thought into the double-indirect pointers which were considerably hard to implement. We had to be careful with math on how we utilized math: `div_round_up` for determing how many blocks are needed, `floor divide` to get the correct table index and `mod` to get the offset within the block.

Finally, we changed are plan for writing past EOF. Initially we thought we would only append on the new written content. However this violated the desired offet we wanted to write at, so we began initializing all data between EOF and the offset to 0. 

In part 3, we ended spending serverals days debugging because we missed a lot of tiny details. The largest issue was memory leaks: when trying to resolve the paths we were constantly opening up more and more inodes without freeing them properly. This led to several days of meticulously walking through our code line by line and resolving the leaks. 

### Reflection

As we had learned from the past, we spent a good amount of time understanding what was given to us before beginning our implementations. This was paticualry useful when attempting part 2. We had understood how `free_map.c` was creating pages for us which was extremely useful when we began creating helpers to check memory, and allocate nonconsecutive blocks for memory. Over the course of the semester in general we learned to really appreciate our desing doc and design reciews. They were pivotal in our understanding of the project and our ability to implement it efficiently. I, Ahmad, even started practicing it in my personal projects and it has lend itself to many improved decisions! I do think I still have a lot to learn here, as was evident in our design doc (we could thought through some of the more complicated edge cases with indirect and double-indirect pointers). However, i'm glad to have been forced to start devloping this habit of writing thorough and well thought out desing docs, something other classes practiced but never to this extent and detail. Thank you couse staff :) 

In terms of collaboration we had a similar split to before. Two of us, Michael and I, tackled part 1 and 2 together. The other two, Sarmad and Joseph, took on part 3. Unfortunately, Sarmad dropped the class this week so Michael and I jumped in to help out with part 3 as well. Despite dropping, Sarmad stuck around for a few days and came to project meetings to help a graceful transition of the code and explain all his design for the remaining algorithms - a spectacular team player.
