/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v152 σ-Knowledge-Distill CLI wrapper.
 */
#include "distill.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *p) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --distill [--n N] [--coverage C] [--seed N]\n"
        "       %s --corpus-info\n", p, p, p);
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 2; }

    if (!strcmp(argv[1], "--self-test")) {
        int rc = cos_v152_self_test();
        printf("{\"kernel\":\"v152\",\"self_test\":%s,\"rc\":%d}\n",
               rc == 0 ? "\"pass\"" : "\"fail\"", rc);
        return rc;
    }

    int do_distill = 0, do_corpus = 0;
    int n_qa = 200;
    float coverage = 0.50f;
    uint64_t seed = 0x152UL;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "--distill"))     do_distill = 1;
        else if (!strcmp(a, "--corpus-info")) do_corpus = 1;
        else if (!strcmp(a, "--n")        && i + 1 < argc) n_qa = atoi(argv[++i]);
        else if (!strcmp(a, "--coverage") && i + 1 < argc) coverage = (float)atof(argv[++i]);
        else if (!strcmp(a, "--seed")     && i + 1 < argc) seed = strtoull(argv[++i], NULL, 0);
        else if (a[0] == '-' && strcmp(a, argv[0]) != 0) { usage(argv[0]); return 2; }
    }

    cos_v152_corpus_t c;
    cos_v152_corpus_seed_default(&c);

    if (do_corpus) {
        char buf[1024];
        cos_v152_corpus_to_json(&c, buf, sizeof(buf));
        printf("%s\n", buf);
        return 0;
    }

    if (do_distill) {
        cos_v152_dataset_t d;
        cos_v152_build_qa(&c, &d, n_qa, seed ^ 0xBADE);
        cos_v152_baseline_sigmas(&d, &c, seed ^ 0xBA51);
        cos_v152_apply_sft(&d, &c, coverage, seed ^ 0xA11C);
        cos_v152_sft_report_t r;
        cos_v152_report(&d, &r);
        char buf[1024];
        cos_v152_report_to_json(&r, buf, sizeof(buf));
        printf("%s\n", buf);
        return r.passed ? 0 : 1;
    }

    usage(argv[0]);
    return 2;
}
