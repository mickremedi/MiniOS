/* Try reading from fd 0 (stdin) and displaying in fd 1 (stdout) */

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
