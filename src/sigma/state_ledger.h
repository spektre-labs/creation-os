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

#define COS_LEDGER_DRIFT_CAP 100

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
    /** Speculative dual-model routing (draft 2B vs verify 9B). */
    int spec_decode_drafts;
    int spec_decode_verifies;
    float cost_total_eur;

    float sigma_mean_session;
    float sigma_mean_delta;
    int omega_generation;

    int pending_actions;
    int max_risk_level;

    int64_t timestamp_ms;

    /** Passive σ drift monitor (rolling window vs calibration baseline). */
    float drift_ring[COS_LEDGER_DRIFT_CAP];
    int   drift_ring_fill;
    int   drift_ring_head;
    float drift_first20[20];
    int   drift_n_first20;
    float drift_baseline_mean;
    float drift_baseline_std;
    signed char drift_baseline_ready;
    signed char drift_baseline_from_env;
    signed char drift_detected;
    signed char drift_stderr_latched;
};

void cos_state_ledger_init(struct cos_state_ledger *l);

void cos_state_ledger_fill_multi(struct cos_state_ledger *l,
                                 const cos_multi_sigma_t *m);

void cos_state_ledger_update(struct cos_state_ledger *l,
                             float sigma_combined,
                             float *meta,
                             int action);

void cos_state_ledger_note_cache_hit(struct cos_state_ledger *l);

/** Count a speculative-decode outcome (draft-only vs escalated verify). */
void cos_state_ledger_note_spec_decode(struct cos_state_ledger *l,
                                       int used_draft);

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
