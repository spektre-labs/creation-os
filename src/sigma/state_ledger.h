/*
 * σ-state ledger — unified live snapshot for σ-gated sessions.
 *
 * Callers populate σ channel scalars via cos_state_ledger_fill_multi()
 * before cos_state_ledger_update() so the JSON snapshot stays coherent.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_STATE_LEDGER_H
#define COS_SIGMA_STATE_LEDGER_H

#include "multi_sigma.h"

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_state_ledger {
    float sigma_logprob;
    float sigma_entropy;
    float sigma_perplexity;
    float sigma_consistency;
    float sigma_combined;

    float meta_perception;
    float meta_self;
    float meta_social;
    float meta_situational;

    float k_eff;
    float dk_dt;
    float d2k_dt2;
    int coherence_status;

    int total_queries;
    int accepts;
    int rethinks;
    int abstains;
    int cache_hits;
    float cost_total_eur;

    float sigma_mean_session;
    float sigma_mean_delta;
    int omega_generation;

    int pending_actions;
    int max_risk_level;

    int64_t timestamp_ms;
};

void cos_state_ledger_init(struct cos_state_ledger *l);

void cos_state_ledger_fill_multi(struct cos_state_ledger *l,
                                 const cos_multi_sigma_t *m);

void cos_state_ledger_update(struct cos_state_ledger *l,
                             float sigma_combined,
                             float *meta,
                             int action);

void cos_state_ledger_note_cache_hit(struct cos_state_ledger *l);

void cos_state_ledger_add_cost(struct cos_state_ledger *l, float eur);

char *cos_state_ledger_to_json(const struct cos_state_ledger *l);

int cos_state_ledger_persist(const struct cos_state_ledger *l,
                             const char *path);

int cos_state_ledger_load(struct cos_state_ledger *l,
                          const char *path);

int cos_state_ledger_default_path(char *buf, size_t cap);

void cos_state_ledger_print_summary(FILE *fp,
                                    const struct cos_state_ledger *l);

int cos_state_ledger_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_STATE_LEDGER_H */
