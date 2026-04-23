/*
 * cos-cache — semantic inference cache inspection (Creation OS edge).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "../sigma/inference_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int infer_cap_from_env(void)
{
    const char *ec = getenv("COS_INFERENCE_CACHE_CAP");
    if (ec != NULL && ec[0] != '\0') {
        int c = atoi(ec);
        if (c >= 8)
            return c;
    }
    return 4096;
}

int main(int argc, char **argv)
{
    int cap = infer_cap_from_env();
    if (cos_inference_cache_init(cap) != 0) {
        fprintf(stderr, "cos-cache: inference cache init failed\n");
        return 1;
    }

    if (argc >= 2 && strcmp(argv[1], "--clear") == 0) {
        cos_inference_cache_clear(1);
        if (cos_inference_cache_save() != 0)
            fprintf(stderr, "cos-cache: warning: save after clear failed\n");
        fprintf(stdout, "Inference cache cleared.\n");
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "--similar") == 0) {
        fprintf(stdout, "Nearest cached responses (Hamming norm):\n");
        cos_inference_cache_dump_similar(argv[2], 16);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "--help") == 0) {
        fprintf(stdout,
                "cos-cache — semantic inference cache\n"
                "  (default)       print statistics\n"
                "  --clear         drop all entries and delete persist file\n"
                "  --similar TEXT  rank cached rows by BSC Hamming similarity\n"
                "  --help          this text\n"
                "  COS_INFERENCE_CACHE_PATH   override persist path\n"
                "  COS_INFERENCE_CACHE_CAP    ring capacity for init\n");
        return 0;
    }

    int            tot = 0, hits = 0, misses = 0;
    float          av = 0.f;
    cos_inference_cache_stats(&tot, &hits, &misses, &av);
    fprintf(stdout,
            "semantic inference cache\n"
            "  entries stored : %d\n"
            "  lookup hits    : %d\n"
            "  lookup misses  : %d\n"
            "  avg latency saved (hits, ms): %.2f\n",
            tot, hits, misses, (double)av);
    return 0;
}
