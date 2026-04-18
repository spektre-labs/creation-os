/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v153 σ-Identity CLI wrapper.
 */
#include "identity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *p) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --evaluate [--seed N]\n"
        "       %s --dump     [--seed N]\n", p, p, p);
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 2; }

    if (!strcmp(argv[1], "--self-test")) {
        int rc = cos_v153_self_test();
        printf("{\"kernel\":\"v153\",\"self_test\":%s,\"rc\":%d}\n",
               rc == 0 ? "\"pass\"" : "\"fail\"", rc);
        return rc;
    }

    int do_eval = 0, do_dump = 0;
    uint64_t seed = 0x153ULL;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "--evaluate")) do_eval = 1;
        else if (!strcmp(a, "--dump")) do_dump = 1;
        else if (!strcmp(a, "--seed") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 0);
        else if (a[0] == '-' && strcmp(a, argv[0]) != 0) { usage(argv[0]); return 2; }
    }

    cos_v153_registry_t r;
    cos_v153_registry_seed_default(&r);
    cos_v153_evaluate(&r, seed);

    if (do_dump) {
        char buf[4096];
        cos_v153_registry_to_json(&r, buf, sizeof(buf));
        printf("%s\n", buf);
        return 0;
    }

    if (do_eval) {
        cos_v153_report_t rep;
        cos_v153_report_compute(&r, &rep);
        char buf[1024];
        cos_v153_report_to_json(&rep, buf, sizeof(buf));
        printf("%s\n", buf);
        return rep.all_contracts ? 0 : 1;
    }

    usage(argv[0]);
    return 2;
}
