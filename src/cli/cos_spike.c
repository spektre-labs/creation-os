/*
 * cos spike — read persisted σ-spike engine snapshot (written by cos chat).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "spike_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int default_snap_path(char *buf, size_t cap)
{
    const char *home = getenv("HOME");
    const char *env  = getenv("COS_SPIKE_SNAPSHOT");
    if (env != NULL && env[0] != '\0') {
        snprintf(buf, cap, "%s", env);
        return 0;
    }
    if (home == NULL || home[0] == '\0')
        return -1;
    snprintf(buf, cap, "%s/.cos/spike_engine.snapshot", home);
    return 0;
}

int main(int argc, char **argv)
{
    struct cos_spike_engine e;

    if (argc >= 2 && strcmp(argv[1], "stats") == 0) {
        char path[512];
        if (default_snap_path(path, sizeof path) != 0) {
            fprintf(stderr, "cos spike stats: HOME unset\n");
            return 2;
        }
        if (cos_spike_engine_init(&e) != 0)
            return 3;
        if (cos_spike_engine_snapshot_read(&e, path) != 0) {
            fprintf(stderr,
                    "cos spike stats: no snapshot at %s "
                    "(run `cos chat --spike` first)\n",
                    path);
            return 4;
        }
        cos_spike_engine_print(stdout, &e);
        return 0;
    }

    fprintf(stderr,
            "usage:\n"
            "  cos spike stats   — print persisted spike statistics\n"
            "  COS_SPIKE_SNAPSHOT   override snapshot path\n");
    return argc == 1 ? 0 : 1;
}
