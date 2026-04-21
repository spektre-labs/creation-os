#include "coherence.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0)
        return cos_ultra_coherence_self_test();
    cos_ultra_coherence_emit_report(stdout, 0.34f, -0.002f, 1.0f, 0.127f);
    return 0;
}
