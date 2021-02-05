#include <stdlib.h>

volatile int a = 0xACFF;

int main() {
    if (a != 0xACFF)
        exit(a);
    return 0;
}
