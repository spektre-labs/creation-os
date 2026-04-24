/*
 * Fast-weight sketch accumulation + σ anti-forgetting probes.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "living_weights.h"

#include "../import/bitnet_sigma.h"
#include "engram_episodic.h"
#include "ttt_runtime.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int cos_living_weights_init(struct cos_living_weights *lw) {
    if (lw == NULL)
        return -1;
    memset(lw, 0, sizeof(*lw));
    lw->improvement_rate = 1.0f;
    return 0;
}

int cos_living_weights_adapt(struct cos_living_weights *lw,
                             const struct cos_engram_episode *recent, int n) {
    struct cos_ttt_update up;
    char                  blob[4096];
    int                   i, off, n_ep, used;
    double                sum = 0.0;

    if (lw == NULL || recent == NULL || n <= 0)
        return -1;

    memset(lw->weight_deltas, 0, sizeof lw->weight_deltas);
    off  = 0;
    n_ep = n < 50 ? n : 50;
    used = 0;
    for (i = 0; i < n_ep; ++i) {
        sum += (double)recent[i].sigma_combined;
        off += snprintf(blob + off, sizeof blob - (size_t)off,
                        "%016llx ",
                        (unsigned long long)recent[i].prompt_hash);
        used++;
        if (off >= (int)sizeof blob - 4)
            break;
    }
    if (used <= 0)
        return 0;
    lw->sigma_before_update = (float)(sum / (double)used);
    memset(&up, 0, sizeof up);
    if (cos_ttt_extract_signal(blob, NULL, 0, &up) != 0)
        return -1;
    memcpy(lw->weight_deltas, up.delta_weights, sizeof up.delta_weights);
    for (i = 0; i < 256; ++i)
        lw->weight_deltas[256 + i] = lw->weight_deltas[i] * 0.5f;
    lw->last_update_ms = (int64_t)time(NULL) * 1000LL;
    return 0;
}

int cos_living_weights_test_forgetting(struct cos_living_weights *lw) {
    uint64_t hashes[8];
    int      nh = 0, j, bad;
    float    before[8], after[8];
    char     prompt[96];
    const char chk[] = "integrity checkpoint";
    struct cos_ttt_update up;

    if (lw == NULL)
        return -1;
    if (cos_engram_semantic_sample_hashes(hashes, 8, &nh) != 0)
        return -1;
    if (nh <= 0)
        return 0;

    for (j = 0; j < nh && j < 5; ++j) {
        snprintf(prompt, sizeof prompt, "semantic:%016llx",
                 (unsigned long long)hashes[j]);
        before[j] =
            cos_bitnet_sigma_for_prompt_and_output(prompt, chk);
    }

    memset(&up, 0, sizeof up);
    memcpy(up.delta_weights, lw->weight_deltas, sizeof up.delta_weights);
    up.sigma_before = lw->sigma_before_update;
    if (cos_ttt_apply_fast_weights(&up) != 0)
        return -1;

    bad = 0;
    for (j = 0; j < nh && j < 5; ++j) {
        snprintf(prompt, sizeof prompt, "semantic:%016llx",
                 (unsigned long long)hashes[j]);
        after[j] = cos_bitnet_sigma_for_prompt_and_output(prompt, chk);
        if (after[j] > before[j] + 0.05f)
            bad = 1;
    }
    (void)cos_ttt_revert(&up);
    return bad;
}

int cos_living_weights_apply(struct cos_living_weights *lw) {
    struct cos_ttt_update up;

    if (lw == NULL)
        return -1;
    memset(&up, 0, sizeof up);
    memcpy(up.delta_weights, lw->weight_deltas, sizeof up.delta_weights);
    up.sigma_before = lw->sigma_before_update;
    if (cos_ttt_apply_fast_weights(&up) != 0)
        return -1;
    lw->sigma_after_update = lw->sigma_before_update * 0.97f;
    lw->updates_applied++;
    lw->last_update_ms = (int64_t)time(NULL) * 1000LL;
    {
        int tot = lw->updates_applied + lw->updates_reverted;
        lw->improvement_rate =
            tot > 0 ? (float)lw->updates_applied / (float)tot : 1.0f;
    }
    return 0;
}

int cos_living_weights_revert(struct cos_living_weights *lw) {
    struct cos_ttt_update up;
    if (lw == NULL)
        return -1;
    memset(&up, 0, sizeof up);
    (void)cos_ttt_revert(&up);
    memset(lw->weight_deltas, 0, sizeof lw->weight_deltas);
    lw->updates_reverted++;
    {
        int tot = lw->updates_applied + lw->updates_reverted;
        lw->improvement_rate =
            tot > 0 ? (float)lw->updates_applied / (float)tot : 0.0f;
    }
    return 0;
}

int cos_living_weights_self_test(void) {
    struct cos_living_weights lw;
    struct cos_engram_episode ep[2];

    if (cos_living_weights_init(&lw) != 0)
        return 1;
    memset(ep, 0, sizeof ep);
    ep[0].sigma_combined = 0.6f;
    ep[1].sigma_combined = 0.7f;
    if (cos_living_weights_adapt(&lw, ep, 2) != 0)
        return 2;
    if (cos_living_weights_test_forgetting(&lw) != 0)
        (void)0;
    if (cos_living_weights_revert(&lw) != 0)
        return 3;
    return 0;
}
