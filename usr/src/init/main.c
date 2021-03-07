#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

char *argv[] = { "sh", 0 };
char *envp[] = { "TEST_ENV=FROM_INIT", 0 };

int
main()
{
    int pid, wpid;

    if (open("console", O_RDWR) < 0) {
        mknod("console", 1, 1);
        open("console", O_RDWR);
    }
    dup(0);                     // stdout
    dup(0);                     // stderr

    while (1) {
        printf("init: starting sh\n");
        pid = fork();
        if (pid < 0) {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0) {
            execve("sh", argv, envp);
            printf("init: exec sh failed\n");
            exit(1);
        }
        while ((wpid = wait(NULL)) >= 0 && wpid != pid)
            printf("zombie!\n");
    }

    return 0;
}
