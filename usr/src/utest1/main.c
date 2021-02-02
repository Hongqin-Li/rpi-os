#include <stdio.h>

volatile int a = 1;

int main() {
    if (a != 1)
        printf("utest1: expect 1, found %d\n", a);
    return 0;
}
