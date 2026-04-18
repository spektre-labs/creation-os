/*
 * v161 σ-Adversarial-Train — CLI.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "adv_train.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fputs(
      "usage: creation_os_v161_adv_train --self-test\n"
      "       creation_os_v161_adv_train --report    [--seed S]\n"
      "       creation_os_v161_adv_train --dpo       [--seed S]\n"
      "       creation_os_v161_adv_train --harden    [--seed S]\n"
      "       creation_os_v161_adv_train --classify  --ent X --neff Y --coh Z\n",
      stderr);
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) {
        int rc = cos_v161_self_test();
        printf("{\"kernel\":\"v161\",\"self_test\":\"%s\",\"rc\":%d}\n",
               rc == 0 ? "pass" : "fail", rc);
        return rc == 0 ? 0 : 1;
    }

    uint64_t seed = 161;
    int do_dpo = 0, do_harden = 0, do_classify = 0;
    float ent = 0.5f, neff = 0.5f, coh = 0.5f;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--report"))   ; /* default */
        else if (!strcmp(argv[i], "--dpo")) do_dpo = 1;
        else if (!strcmp(argv[i], "--harden")) do_harden = 1;
        else if (!strcmp(argv[i], "--classify")) do_classify = 1;
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--ent") && i + 1 < argc) ent = strtof(argv[++i], NULL);
        else if (!strcmp(argv[i], "--neff") && i + 1 < argc) neff = strtof(argv[++i], NULL);
        else if (!strcmp(argv[i], "--coh") && i + 1 < argc) coh = strtof(argv[++i], NULL);
        else return usage();
    }

    cos_v161_trainer_t t;
    cos_v161_trainer_init(&t, seed);

    if (do_dpo) {
        cos_v161_dpo_pair_t pairs[COS_V161_MAX_ATTACKS];
        int n = cos_v161_trainer_build_dpo(&t, pairs, COS_V161_MAX_ATTACKS);
        printf("{\"kernel\":\"v161\",\"n_pairs\":%d,\"pairs\":[", n);
        for (int i = 0; i < n; ++i) {
            if (i) printf(",");
            printf("{\"type\":\"%s\",\"sigma_refuse\":%.4f,"
                   "\"chosen\":\"%s\"}",
                   cos_v161_attack_name(pairs[i].type),
                   pairs[i].sigma_refuse, pairs[i].chosen);
        }
        printf("]}\n");
        return 0;
    }

    if (do_classify) {
        float score = 0.0f;
        cos_v161_attack_type_t c = cos_v161_classify(&t, ent, neff, coh, &score);
        printf("{\"kernel\":\"v161\",\"classified\":\"%s\",\"score\":%.4f}\n",
               cos_v161_attack_name(c), score);
        return 0;
    }

    if (do_harden) {
        int nc = cos_v161_trainer_harden(&t);
        (void)nc;
    }

    char buf[32768];
    cos_v161_trainer_to_json(&t, buf, sizeof(buf));
    printf("%s\n", buf);
    return 0;
}
