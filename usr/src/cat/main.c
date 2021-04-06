#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int
main(int argc, char *argv[])
{
    int fd, c, n, i, j;
    char buf[512];

    for (i = 1; i < argc; i++) {
        if ((fd = open(argv[i], O_RDONLY)) < 0) {
            fprintf(stderr, "%s: %s : %s\n", argv[0], argv[i],
                    strerror(errno));
            continue;
        }
        while ((n = read(fd, buf, 512)) > 0) {
            for (j = 0; j < n; j++)
                fprintf(stdout, "%c", buf[j]);
        }
        close(fd);
    }
    return 0;
}
