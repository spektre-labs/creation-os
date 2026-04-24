#include "constitution.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PA(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL %s\n", msg); \
            return 1; \
        } \
    } while (0)

int main(void)
{
    PA(cos_constitution_self_test() == 0, "self_test");
    return 0;
}
