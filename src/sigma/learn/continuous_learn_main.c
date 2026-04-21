#include "continuous_learn.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--self-test") == 0)
            return cos_ultra_continuous_learn_self_test();
    }
    if (argc >= 2 && strcmp(argv[1], "--status") == 0) {
        cos_ultra_continuous_emit_status(stdout);
        return 0;
    }
    cos_ultra_continuous_emit_status(stdout);
    return 0;
}
