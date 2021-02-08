#include <stdlib.h>

volatile int a = 0xBB00;

int
main()
{
    if (a != 0xBB00)
        exit(a);
    return 0;
}
