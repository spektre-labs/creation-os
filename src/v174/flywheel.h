/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v174 σ-Flywheel — self-evolving synthetic-data pipeline.
 *
 * v124 continual learns from usage.  v125 DPO learns from
 * preferences.  v141 curriculum learns from weaknesses.  v174
 * composes them into a single closed loop:
 *
 *     proposer  ─►  solver  ─►  σ-verifier  ─►  DPO data
 *         ▲                           │
 *         │                           ▼
 *     diversity       v124 hot-swap + v143 smoke ──┐
 *     controller                                   │
 *         └───────────  anti-collapse guards  ◄────┘
 *
 * The critical new idea: **σ is a data filter**.
 *
 *   σ < τ_confident   → chosen        (clean positive)
 *   σ > τ_uncertain   → pair with fix (negative + big-model chosen)
 *   τ_c ≤ σ ≤ τ_u     → SKIP          (uninformative)
 *
 * Only *informative* samples survive into the DPO set.  Plus
 * three anti-model-collapse guards that halt the flywheel if:
 *
 *   a) answer entropy H shrinks below H_min
 *   b) v143 benchmark regresses vs the rolling baseline
 *   c) σ-distribution variance shrinks below var_min
 *
 * v174.0 (this file) ships a deterministic, weights-free
 * fixture: the proposer generates 50 synthetic questions in
 * the v141 weakest-domain, the solver assigns a σ to each,
 * a diversity controller checks coverage across 5 embedding
 * clusters, and the anti-collapse guards run every cycle.
 *
 * v174.1 (named, not shipped):
 *   - real v151 code-agent question generation
 *   - real v114 swarm big-model corrections
 *   - real v125 DPO training + v124 hot-swap
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V174_FLYWHEEL_H
#define COS_V174_FLYWHEEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V174_N_QUESTIONS 50
#define COS_V174_N_CLUSTERS   5
#define COS_V174_MAX_STR    128

typedef enum {
    COS_V174_CLASS_CHOSEN = 0,   /* σ < τ_confident */
    COS_V174_CLASS_PAIR   = 1,   /* σ > τ_uncertain (with correction) */
    COS_V174_CLASS_SKIP   = 2,   /* uninformative */
} cos_v174_class_t;

typedef struct {
    int              id;
    char             prompt[COS_V174_MAX_STR];
    int              cluster;                /* 0..N_CLUSTERS-1 */
    float            sigma_answer;
    float            sigma_big_model;        /* for PAIR class */
    cos_v174_class_t cls;
} cos_v174_sample_t;

typedef struct {
    int    n_chosen;
    int    n_pair;
    int    n_skip;
    float  entropy_answers;      /* Shannon H over cluster distribution */
    float  sigma_variance;       /* variance of σ_answer across samples */
    int    distinct_clusters;    /* how many of N_CLUSTERS saw ≥ 1 sample */
    float  benchmark_score;      /* synthetic v143-smoke score */
    float  benchmark_baseline;   /* rolling */
    bool   collapse_triggered;   /* any guard fired */
    char   collapse_reason[COS_V174_MAX_STR];
} cos_v174_cycle_t;

typedef struct {
    float               tau_confident;      /* default 0.25 */
    float               tau_uncertain;      /* default 0.60 */
    float               h_min;              /* default 1.20 (nats) */
    float               var_min;            /* default 0.010 */
    float               regression_slack;   /* default 0.05 */

    int                 n_samples;
    cos_v174_sample_t   samples[COS_V174_N_QUESTIONS];

    cos_v174_cycle_t    cycle;
    uint64_t            seed;
} cos_v174_state_t;

void cos_v174_init(cos_v174_state_t *s, uint64_t seed);

/* One end-to-end flywheel cycle: generate → solve → σ-classify
 * → diversity → anti-collapse guards → record.  Returns 0 on
 * success, or a non-zero stage on collapse trigger. */
int  cos_v174_run_cycle(cos_v174_state_t *s, float baseline);

/* Classify a single σ — exposed for unit testing. */
cos_v174_class_t cos_v174_classify(const cos_v174_state_t *s, float sigma);
const char *     cos_v174_class_name(cos_v174_class_t c);

size_t cos_v174_to_json(const cos_v174_state_t *s, char *buf, size_t cap);
size_t cos_v174_dpo_ndjson(const cos_v174_state_t *s,
                           char *buf, size_t cap);

int cos_v174_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V174_FLYWHEEL_H */
