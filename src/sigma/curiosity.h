/*
 * σ-native curiosity — high σ domains rank as most worth learning.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_CURIOSITY_H
#define COS_SIGMA_CURIOSITY_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_curiosity_target {
    uint64_t domain_hash;
    char     topic[256];
    float    sigma_mean;
    float    learning_rate;
    float    curiosity_score;
    int      attempts;
    int      improvements;
};

int cos_curiosity_init(void);

void cos_curiosity_shutdown(void);

int cos_curiosity_rank(struct cos_curiosity_target *targets, int max, int *n);

int cos_curiosity_explore(const struct cos_curiosity_target *target);

int cos_curiosity_update(uint64_t domain_hash, float sigma_before,
                         float sigma_after);

float cos_curiosity_score(float sigma_mean, int encounters, int attempts);

/** Pretty-print top ranked rows (for cos introspect). */
void cos_curiosity_fprint_queue(FILE *fp, int max_rows);

int cos_curiosity_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_CURIOSITY_H */
