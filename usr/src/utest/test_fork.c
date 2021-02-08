#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

void
fork1()
{
    int pid = fork();
    if (!pid)
        exit(0);
}

void
test_fork()
{
    int n = 10;
    for (int i = 0; i < n; i++)
        fork1();
    for (int i = 0; i < n; i++)
        wait(NULL);
}
