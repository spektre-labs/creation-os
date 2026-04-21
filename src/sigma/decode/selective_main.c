#include "selective.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0)
        return cos_ultra_selective_self_test();
    fprintf(stderr, "usage: creation_os_ultra_selective --self-test\n");
    return 2;
}
