/*
 * cos self-play — CLI for σ-measured challenge/response self-play.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "self_play.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    fprintf(stderr,
            "cos self-play — domain self-play (BSC agreement + engram)\n"
            "  --domain TEXT   target domain (default: general)\n"
            "  --rounds N      number of rounds (default: 1, max 256)\n"
            "  --self-test     run in-process self-test and exit 0/1\n"
            "  --help          this text\n");
}

int main(int argc, char **argv)
{
    const char *domain  = "general";
    int         rounds  = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--self-test") == 0) {
            return cos_self_play_self_test() != 0 ? 1 : 0;
        } else if (strcmp(argv[i], "--domain") == 0 && i + 1 < argc) {
            domain = argv[++i];
        } else if (strcmp(argv[i], "--rounds") == 0 && i + 1 < argc) {
            rounds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        }
    }

    if (rounds < 1)
        rounds = 1;
    if (rounds > 256)
        rounds = 256;

    struct cos_self_play_round *r =
        (struct cos_self_play_round *)calloc(
            (size_t)rounds, sizeof(struct cos_self_play_round));
    if (!r) {
        fprintf(stderr, "cos self-play: OOM\n");
        return 1;
    }

    int rc = cos_self_play_run(domain, rounds, r);
    cos_self_play_print_report(r, rounds);
    free(r);
    if (rc == 0)
        return 0;
    if (rc == -3)
        fprintf(stderr, "cos self-play: some rounds failed (local pipeline)\n");
    return 1;
}
