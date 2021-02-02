#include <stdio.h>

volatile int a = 2;

int main() {
    if (a != 2)
        printf("utest2: expect 2, found %d\n", a);
    return 0;
}
