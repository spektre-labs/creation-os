/*
 * cos-federation CLI — status / share / peers / forget / config display.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "../sigma/federation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *fp)
{
    fputs(
        "cos-federation — σ-gated federated semantic sharing\n"
        "  cos federation status\n"
        "  cos federation peers\n"
        "  cos federation share     (placeholder: packages local summaries)\n"
        "  cos federation forget HASH\n"
        "  cos federation config\n",
        fp);
}

int cos_federation_main(int argc, char **argv)
{
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    if (cos_federation_init(NULL) != 0) {
        fputs("federation init failed\n", stderr);
        return 1;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(stdout);
        return 0;
    }
    if (strcmp(argv[1], "status") == 0) {
        cos_federation_print_status(stdout);
        return 0;
    }
    if (strcmp(argv[1], "peers") == 0) {
        uint64_t ids[32];
        float    tr[32];
        int      n = 0;
        cos_federation_peers(ids, tr, 32, &n);
        for (int i = 0; i < n; ++i)
            printf("%016llx trust=%.4f\n", (unsigned long long)ids[i],
                   (double)tr[i]);
        return 0;
    }
    if (strcmp(argv[1], "config") == 0) {
        cos_federation_print_status(stdout);
        return 0;
    }
    if (strcmp(argv[1], "forget") == 0 && argc >= 3) {
        unsigned long long fh = strtoull(argv[2], NULL, 0);
        int rc = cos_federation_broadcast_forget((uint64_t)fh, "cli forget");
        return rc != 0 ? 3 : 0;
    }
    if (strcmp(argv[1], "share") == 0) {
        fputs("share: use library hooks after consolidate / distill (see "
              "federation.h)\n",
              stdout);
        return 0;
    }
    usage(stderr);
    return 2;
}

#ifdef COS_FEDERATION_MAIN
int main(int argc, char **argv)
{
    return cos_federation_main(argc, argv);
}
#endif
