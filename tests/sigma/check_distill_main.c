/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#define CREATION_OS_ENABLE_SELF_TESTS 1
#define CREATION_OS_DISTILL_TEST 1

#include "../../src/sigma/distill.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    int nd = 0, sk = 0;
    assert(cos_distill_batch("/nonexistent_path_distill_xyz", NULL, &nd, &sk)
           == -1);

    char prev[512];
    prev[0] = '\0';
    const char *oh = getenv("HOME");
    if (oh) {
        snprintf(prev, sizeof prev, "%s", oh);
    }
    assert(setenv("HOME", "/tmp", 1) == 0);
    assert(setenv("COS_BITNET_SERVER_EXTERNAL", "1", 1) == 0);
    assert(setenv("COS_BITNET_SERVER_PORT", "65530", 1) == 0);
    cos_distill_shutdown();

    assert(cos_distill_init() == 0);
    assert(cos_distill_teacher_count() >= 4);

    struct cos_distill_teacher tr[12];
    int nf = 0;
    cos_distill_teacher_ranking(tr, 12, &nf);
    assert(nf > 0);

    assert(cos_distill_self_test() == 0);

    long nr;
    double ai, ast;
    assert(cos_distill_stats(&nr, &ai, &ast) == 0);

    char export_path[128];
    snprintf(export_path, sizeof export_path, "/tmp/distill_ck_%ld.jsonl",
             (long)getpid());
    assert(cos_distill_export_training_data(
               export_path, (int)COS_DISTILL_EXPORT_ALPACA)
           == 0);

    assert(cos_distill_fprint_history(stdout, 3) == 0);

    cos_distill_shutdown();
    if (prev[0]) (void)setenv("HOME", prev, 1);
    return 0;
}
