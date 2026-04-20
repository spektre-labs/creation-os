/*
 * σ-Federation — σ-weighted Δweight aggregation for mesh learning.
 *
 * Each node trains locally (P6 TTT, σ-curriculum) on data that
 * never leaves the device; only Δweight vectors cross the wire.
 * σ-Federation is the aggregator: given N updates, compute a
 * single Δw_global that respects both trust (lower σ ⇒ higher
 * weight) and sample count (more samples ⇒ higher weight), while
 * rejecting poisoned or noisy updates.
 *
 * Weight per contributor:
 *
 *     w_i = (1 − σ_i) · n_i
 *
 * A σ_i ≥ σ_reject floor disqualifies the update entirely (treated
 * as malicious / too unreliable to average in).  If every surviving
 * contributor carries w_i = 0 the aggregator returns a zero-change
 * Δw_global and reports `abstained = 1`.
 *
 * Aggregate:
 *
 *     α_i = w_i / Σ w_j
 *     Δw_global[j] = Σ_i α_i · Δw_i[j]
 *
 * Poison guard (L2 norm):
 *     ‖Δw_i‖₂ ≤ poison_max_norm_scale · median(‖Δw‖₂)
 *
 * A single contributor whose Δweight norm is several× the cohort
 * median is flagged `flagged_poison = 1` and dropped even if
 * their σ looks fine.
 *
 * Contracts (v0):
 *   1. init enforces NULL-guards, positive capacity, valid
 *      thresholds (σ_reject ∈ (0, 1], poison_scale ≥ 1.0).
 *   2. add_update copies Δw into caller-owned storage; returns
 *      −3 if capacity exhausted, −4 on NaN detection.
 *   3. aggregate reports: n_accepted, n_rejected_sigma,
 *      n_rejected_poison, total_weight, abstained flag.
 *   4. Deterministic: no RNG.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_FEDERATION_H
#define COS_SIGMA_FEDERATION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_FED_NODE_ID_CAP 32

typedef struct {
    char   node_id[COS_FED_NODE_ID_CAP];
    const float *delta_w;    /* borrowed; caller keeps alive    */
    int    n_params;
    float  sigma_node;        /* 0..1 — lower = more trusted     */
    int    n_samples;         /* how many datapoints produced Δw */
    int    accepted;
    int    flagged_sigma;
    int    flagged_poison;
    float  norm_l2;
    float  weight;            /* (1-σ)·n_samples, post-sanitise  */
} cos_fed_update_t;

typedef struct {
    cos_fed_update_t *slots;
    int               cap;
    int               count;
    int               n_params;        /* fixed after first add    */
    float             sigma_reject;
    float             poison_max_scale;
} cos_fed_t;

typedef struct {
    int    n_accepted;
    int    n_rejected_sigma;
    int    n_rejected_poison;
    int    abstained;           /* Σ w = 0 after filtering         */
    float  total_weight;
    float  median_norm;
    float  max_norm;
    float  delta_l2_norm;       /* of the aggregated update        */
} cos_fed_report_t;

int  cos_sigma_fed_init(cos_fed_t *fed,
                        cos_fed_update_t *storage, int cap,
                        float sigma_reject,
                        float poison_max_scale);

/* Register a local update.  `n_params` must match across calls. */
int  cos_sigma_fed_add(cos_fed_t *fed,
                       const char *node_id,
                       const float *delta_w, int n_params,
                       float sigma_node, int n_samples);

/* Compute Δw_global in `out_delta` of length == fed->n_params.
 * Returns 0 on success (including abstain); <0 on arg error. */
int  cos_sigma_fed_aggregate(cos_fed_t *fed,
                             float *out_delta,
                             cos_fed_report_t *out_report);

int  cos_sigma_fed_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_FEDERATION_H */
