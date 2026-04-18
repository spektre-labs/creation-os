/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v143 σ-Benchmark — CLI.
 */
#include "benchmark.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *a0) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --run [--seed N] [--out PATH]\n",
        a0, a0);
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);

    if (!strcmp(argv[1], "--self-test")) return cos_v143_self_test();
    if (!strcmp(argv[1], "--run")) {
        uint64_t seed = 0xC057BBULL;
        const char *out = NULL;
        for (int i = 2; i < argc; ++i) {
            if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
                seed = (uint64_t)strtoull(argv[++i], NULL, 0);
            } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
                out = argv[++i];
            }
        }
        cos_v143_result_t r;
        if (cos_v143_run_all(seed, &r) != 0) return 1;
        char buf[2048];
        cos_v143_to_json(&r, buf, sizeof buf);
        printf("%s\n", buf);
        if (out) {
            if (cos_v143_write_json(&r, out) != 0) {
                fprintf(stderr, "v143: failed to write %s\n", out);
                return 2;
            }
            fprintf(stderr, "v143: wrote %s\n", out);
        }
        return 0;
    }
    return usage(argv[0]);
}
