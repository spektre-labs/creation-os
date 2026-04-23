/*
 * σ-measured self-play — dual-sample consistency over the local pipeline.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SELF_PLAY_H
#define COS_SIGMA_SELF_PLAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_self_play_round {
    char    prompt[1024];
    char    answer[4096];
    float   sigma;
    int     verified; /* -1 unknown  0 inconsistent  1 consistent */
};

int cos_self_play_run(const char *domain, int n_rounds,
                      struct cos_self_play_round *results);

void cos_self_play_print_report(const struct cos_self_play_round *results,
                                int n);

int cos_self_play_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_SELF_PLAY_H */
