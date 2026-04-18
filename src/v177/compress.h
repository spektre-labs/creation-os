/*
 * v177 σ-Compress — σ-guided model compression.
 *
 *   Given a synthetic BitNet-2B-like layer stack, v177
 *   demonstrates three σ-aware compression stages:
 *
 *     1. σ-aware pruning:    drop neurons whose removal
 *                             leaves σ unchanged.
 *     2. σ-aware mixed prec: keep critical layers (high
 *                             σ-impact) at INT8, demote
 *                             others to INT4 (or INT2).
 *     3. σ-layer merging:    collapse adjacent layers that
 *                             share a σ-profile.
 *
 *   The exit invariant: total σ-calibration drift ≤ 5 %
 *   after compression, while parameter count and runtime
 *   footprint drop sharply.
 *
 * v177.0 (this file) ships a closed-form model: 16 layers
 * × 64 neurons/layer with a σ-impact table sampled from a
 * deterministic distribution.  The three passes operate on
 * this state and produce exact before/after metrics.
 *
 * v177.1 (named, not shipped):
 *   - real BitNet-2B prune + emit models/v177/bitnet_1b_sigma_pruned.gguf
 *   - real activation quantisation INT8 → INT4/INT2
 *   - real layer-merge via weight/bias fusion
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V177_COMPRESS_H
#define COS_V177_COMPRESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V177_N_LAYERS   16
#define COS_V177_N_NEURONS  64

typedef enum {
    COS_V177_PREC_INT8 = 0,
    COS_V177_PREC_INT4 = 1,
    COS_V177_PREC_INT2 = 2,
} cos_v177_prec_t;

typedef struct {
    int             id;
    /* sigma_impact[n] ∈ [0,1]: how much σ rises if neuron n is pruned */
    float           sigma_impact[COS_V177_N_NEURONS];
    bool            pruned[COS_V177_N_NEURONS];
    int             n_alive;            /* count of non-pruned neurons */
    float           sigma_layer;        /* fitted σ for the layer */
    cos_v177_prec_t precision;
    bool            merged_into_prev;
} cos_v177_layer_t;

typedef struct {
    size_t params_before;
    size_t params_after;
    size_t bytes_before;
    size_t bytes_after;

    float  sigma_before;       /* calibration σ before compression */
    float  sigma_after;        /* calibration σ after compression  */
    float  sigma_drift_pct;    /* (|after − before|/before) · 100  */
    int    n_layers_before;
    int    n_layers_after;
    int    n_neurons_pruned;
    int    n_layers_int8;
    int    n_layers_int4;
    int    n_layers_int2;
    int    n_layers_merged;
} cos_v177_report_t;

typedef struct {
    cos_v177_layer_t  layers[COS_V177_N_LAYERS];
    cos_v177_report_t report;

    float    prune_tau;        /* prune neuron if σ_impact < this */
    float    int8_tau;          /* keep INT8 if σ_layer ≤ this */
    float    int4_tau;          /* INT4 between int8_tau and int4_tau */
    float    merge_tau;         /* merge if |Δσ| ≤ this */
    float    drift_budget_pct;  /* default 5.0 */
    uint64_t seed;
} cos_v177_state_t;

void cos_v177_init(cos_v177_state_t *s, uint64_t seed);

void cos_v177_build_fixture    (cos_v177_state_t *s);
void cos_v177_prune            (cos_v177_state_t *s);
void cos_v177_mixed_precision  (cos_v177_state_t *s);
void cos_v177_merge_layers     (cos_v177_state_t *s);
void cos_v177_report           (cos_v177_state_t *s);

size_t cos_v177_to_json(const cos_v177_state_t *s, char *buf, size_t cap);

int cos_v177_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V177_COMPRESS_H */
