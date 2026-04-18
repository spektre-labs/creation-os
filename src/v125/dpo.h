/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v125 σ-DPO — Direct Preference Optimization with σ-derived labels.
 *
 * Classical DPO (Rafailov et al., 2023) needs human-written preference
 * pairs (y_w "chosen", y_l "rejected") gathered via RLHF-style
 * annotation.  That annotation stage is exactly what "RLHF as Opinion
 * Laundering" (Rainio 2025) pulls apart: the annotator's σ becomes a
 * political prior that disappears into the policy weights.
 *
 * v125 removes the annotator.  The preference signal *is* σ:
 *
 *   rule A — correction pair:
 *       user_corrected && correction  →
 *           chosen   = correction
 *           rejected = response
 *           source   = CORRECTION
 *
 *   rule B — σ pair:
 *       two rows sharing a context key, one with
 *       σ_product < τ_low, the other with σ_product > τ_high →
 *           chosen   = low-σ row
 *           rejected = high-σ row
 *           source   = SIGMA
 *
 * No human reviewer.  No reward model.  No PPO.  The gradient flows
 * from the model's own coherence measurement on its own output.
 *
 * DPO loss (numerically stable form):
 *
 *   δ = β · ( (log π(yw|x) − log π_ref(yw|x))
 *           − (log π(yl|x) − log π_ref(yl|x)) )
 *   L = −log σ(δ) = softplus(−δ)
 *
 * with default β = 0.1 to stay in the conservative regime (no mode
 * collapse).  The caller supplies the four log-probs from the MLX
 * forward pass; v125.0 is pure math so the merge-gate can exercise
 * it without weights.  v125.1 wires the MLX LoRA adapter stacked on
 * top of the v124 continual-learning adapter.
 *
 * Mode-collapse detector: we compare the σ distribution before and
 * after a DPO epoch.  If stddev or Shannon entropy collapses
 * disproportionately, we flag the epoch and the caller rolls back —
 * this is the same safety pattern v124 uses for baseline accuracy.
 *
 * This file is the policy + label logic + loss kernel + mode-
 * collapse check.  Everything deterministic, everything in pure C.
 */
#ifndef COS_V125_DPO_H
#define COS_V125_DPO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V125_MAX_FIELD     512
#define COS_V125_CONTEXT_KEY    64

#define COS_V125_TAU_LOW_DEFAULT     0.30f
#define COS_V125_TAU_HIGH_DEFAULT    0.60f
#define COS_V125_BETA_DEFAULT        0.10f
#define COS_V125_STD_COLLAPSE_DEF    0.40f   /* std_after/std_before < 0.40 → collapse */
#define COS_V125_ENT_COLLAPSE_DEF    0.60f   /* entropy_after/entropy_before < 0.60  */
#define COS_V125_SIGMA_BINS          10

typedef struct cos_v125_config {
    float tau_low;                /* 0.30 */
    float tau_high;               /* 0.60 */
    float beta;                   /* 0.10 */
    float std_collapse_ratio;     /* 0.40 */
    float entropy_collapse_ratio; /* 0.60 */
} cos_v125_config_t;

typedef struct cos_v125_interaction {
    char  prompt     [COS_V125_MAX_FIELD];
    char  context_key[COS_V125_CONTEXT_KEY]; /* "capital-of-france" */
    char  response   [COS_V125_MAX_FIELD];
    float sigma_product;
    int   user_corrected;                    /* 0 | 1 */
    char  correction [COS_V125_MAX_FIELD];
} cos_v125_interaction_t;

typedef enum {
    COS_V125_PAIR_CORRECTION = 1,
    COS_V125_PAIR_SIGMA      = 2,
} cos_v125_pair_source_t;

typedef struct cos_v125_pair {
    char  prompt  [COS_V125_MAX_FIELD];
    char  chosen  [COS_V125_MAX_FIELD];
    char  rejected[COS_V125_MAX_FIELD];
    float sigma_chosen;
    float sigma_rejected;
    cos_v125_pair_source_t source;
} cos_v125_pair_t;

typedef struct cos_v125_dataset_stats {
    int   interactions_read;
    int   pairs_emitted;
    int   pairs_from_correction;
    int   pairs_from_sigma;
    int   rows_skipped_mid_sigma;    /* neither chosen nor rejected */
    int   rows_skipped_unpaired;     /* no context-key match */
    float mean_sigma_chosen;         /* over emitted pairs */
    float mean_sigma_rejected;
} cos_v125_dataset_stats_t;

/* ----- Fill defaults. ----------------------------------------------- */
void cos_v125_config_defaults(cos_v125_config_t *cfg);

/* ----- σ-label a dataset into preference pairs. --------------------- *
 * Walks `items[0..n)` once.  Emits up to `max_pairs` pairs into `out`.
 * `stats` is filled with per-run counters.  Returns number of pairs
 * emitted (≤ max_pairs).
 *
 * Deterministic first-match pairing: for a high-σ unmatched row we
 * pick the earliest *unused* low-σ row that shares the context_key.
 * Each low-σ row is used at most once per epoch.  Correction pairs
 * are emitted regardless of context_key matching.                   */
int cos_v125_label_dataset(const cos_v125_config_t *cfg,
                           const cos_v125_interaction_t *items,
                           int n,
                           cos_v125_pair_t *out,
                           int max_pairs,
                           cos_v125_dataset_stats_t *stats);

/* ----- DPO loss kernel (numerically stable). ------------------------ */
typedef struct cos_v125_logprobs {
    float logp_chosen;         /* log π_policy(y_w | x) */
    float logp_rejected;       /* log π_policy(y_l | x) */
    float logp_ref_chosen;     /* log π_ref   (y_w | x) */
    float logp_ref_rejected;   /* log π_ref   (y_l | x) */
} cos_v125_logprobs_t;

/* Returns L = softplus(−δ); writes sigmoid(δ) to `*pref_prob` if
 * non-NULL.  `beta` is the DPO temperature. */
float cos_v125_dpo_loss(float beta,
                        const cos_v125_logprobs_t *lp,
                        float *pref_prob);

/* Mean loss over a batch. */
float cos_v125_dpo_batch_loss(float beta,
                              const cos_v125_logprobs_t *batch,
                              int n);

/* ----- σ distribution / mode-collapse detection. ------------------- */
typedef struct cos_v125_distribution_stats {
    float mean;
    float stddev;
    float entropy_bits;   /* Shannon entropy of 10-bin σ histogram */
    int   n;
} cos_v125_distribution_stats_t;

void cos_v125_sigma_distribution(const float *sigmas, int n,
                                 cos_v125_distribution_stats_t *out);

typedef enum {
    COS_V125_MODE_OK       = 0,
    COS_V125_MODE_COLLAPSE = 1,
} cos_v125_mode_t;

cos_v125_mode_t
cos_v125_check_mode_collapse(const cos_v125_config_t *cfg,
                             const cos_v125_distribution_stats_t *before,
                             const cos_v125_distribution_stats_t *after);

/* ----- JSON serializers. ------------------------------------------- */
int  cos_v125_stats_to_json(const cos_v125_dataset_stats_t *s,
                            const cos_v125_config_t *cfg,
                            char *out, size_t cap);

int  cos_v125_distribution_to_json(const cos_v125_distribution_stats_t *d,
                                   char *out, size_t cap);

int  cos_v125_self_test(void);

#ifdef __cplusplus
}
#endif
#endif  /* COS_V125_DPO_H */
