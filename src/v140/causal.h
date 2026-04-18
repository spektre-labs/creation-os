/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v140 σ-Causal — counterfactual propagation and per-channel
 * σ-ablation attribution.
 *
 * v139 answers "what happens next".  v140 answers "what would
 * happen IF".  Two small pieces composed on top of v139's linear
 * A:
 *
 *   1. Counterfactual simulation (do-calculus, linear edition).
 *        s_do[t]      = s_nat[t] with dim_k overwritten by `value`
 *        s_do[t+1]    = A · s_do[t]
 *        s_nat[t+1]   = A · s_nat[t]
 *        σ_causal     = ‖s_do[t+1] − s_nat[t+1]‖ / max(‖s_nat[t+1]‖, ε)
 *      σ_causal is the magnitude of the interventional effect —
 *      a non-zero σ_causal identifies a dimension whose change
 *      matters; zero means the dimension is causally inert under
 *      the learned A.
 *
 *   2. σ-channel ablation attribution.
 *        Given an 8-channel σ-vector σ[0..7] and the current
 *        σ-aggregator (weighted geometric mean with weights w and
 *        threshold τ), compute the baseline σ_agg.  Then, for each
 *        channel i, ablate (set σ[i] = 1.0, the "no signal" value)
 *        and recompute σ_agg_i.  Δ_i = σ_agg − σ_agg_i is the
 *        channel's contribution to the decision.  The kernel
 *        returns the top-3 contributors in descending |Δ| with
 *        their percent-of-total attribution ("abstain was 62%
 *        n_effective, 28% tail_mass, 10% entropy").
 *
 * v140.0 (this commit) is pure C, deterministic, and composes with
 * v139 by calling its predict primitive.  v140.1 broadens the
 * intervention set from "overwrite a single dim" to "structural
 * intervention on the σ-aggregator itself" and wires /v1/explain
 * into v106.
 */
#ifndef COS_V140_CAUSAL_H
#define COS_V140_CAUSAL_H

#include "world_model.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 1. Counterfactual propagation ---------------------------- */

typedef struct cos_v140_counterfactual {
    int     dim;
    int     intervened_index;  /* which component was overwritten   */
    float   intervened_value;  /* the overwriting value             */
    float   delta_norm;        /* ‖s_do[t+1] − s_nat[t+1]‖          */
    float   natural_norm;      /* ‖s_nat[t+1]‖                      */
    float   sigma_causal;      /* delta_norm / max(natural_norm, ε) */
} cos_v140_counterfactual_t;

int   cos_v140_counterfactual(const cos_v139_model_t *m,
                              const float *state_now,
                              int index, float value,
                              cos_v140_counterfactual_t *out);

/* ---- 2. σ-channel ablation attribution ------------------------ */

#define COS_V140_MAX_CHANNELS 8
#define COS_V140_TOPK         3

typedef struct cos_v140_channel_attr {
    int    index;         /* channel id [0..7]                      */
    float  delta;         /* σ_agg_baseline − σ_agg_ablated         */
    float  percent;       /* 100 · |delta| / Σ|delta_j|             */
} cos_v140_channel_attr_t;

typedef struct cos_v140_attribution {
    int    n_channels;
    float  weights[COS_V140_MAX_CHANNELS];
    float  sigma[COS_V140_MAX_CHANNELS];
    float  sigma_agg;                /* baseline weighted geo-mean  */
    float  threshold;                /* τ                            */
    int    verdict_emit;             /* σ_agg < τ ⇒ emit             */
    int    top_k;                    /* ≤ COS_V140_TOPK              */
    cos_v140_channel_attr_t top[COS_V140_TOPK];
} cos_v140_attribution_t;

/* Compute the weighted geometric mean of σ-channels.
 * w[i] ≥ 0, Σ w[i] > 0, σ[i] ∈ (0, 1].  σ[i] = 0 is clamped to 1e-6. */
float cos_v140_weighted_geomean(const float *w, const float *s, int n);

/* Run ablation attribution.  Returns top-3 channels sorted by
 * descending |Δ|, with percent-of-total attribution. */
int   cos_v140_attribute(const float *w, const float *sigma, int n,
                         float threshold,
                         cos_v140_attribution_t *out);

/* JSON summary of attribution: {"sigma_agg":…,"verdict":"emit",
 * "top":[{"index":..,"delta":..,"percent":..},…]}. */
int   cos_v140_attribution_to_json(const cos_v140_attribution_t *a,
                                   char *buf, size_t cap);

int   cos_v140_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
