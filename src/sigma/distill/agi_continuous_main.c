/* AGI-3 — `cos distill status` driver. */

#include "agi_continuous.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0)
        return cos_agi_distill_self_test() != 0 ? 1 : 0;
    if (argc >= 2 && (strcmp(argv[1], "status") == 0 ||
                      strcmp(argv[1], "--status") == 0)) {
        if (cos_agi_distill_emit_status(stdout) != 0) return 1;
        return 0;
    }
    /* Default: same as `distill status`. */
    if (cos_agi_distill_emit_status(stdout) != 0) return 1;
    return 0;
}
