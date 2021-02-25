#include <stdio.h>

int
main() {
    int nbuf = 512;
    char buf[nbuf];
    char *s;

    while ((s = fgets(buf, nbuf, stdin)) != NULL)
        printf("%s", s);

    return 0;
}
