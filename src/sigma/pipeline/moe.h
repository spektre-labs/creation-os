/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-MoE — σ-gated expert routing with dynamic width (v280 live).
 *
 * Classical Mixture-of-Experts picks an expert via argmax over router
 * logits.  Every active expert runs at full width (100% of its FFN
 * dim).  That wastes compute on easy tokens.
 *
 * σ-MoE (MoSE, arxiv 2602.06154) uses the σ measurement to *also*
 * decide how much of the chosen expert to run:
 *
 *     σ < 0.15  →  width = 0.25  (quarter)   — easy token, 4× faster
 *     σ < 0.30  →  width = 0.50  (half)      — mild uncertainty
 *     σ < 0.50  →  width = 0.75  (three-q)   — moderate uncertainty
 *     else      →  width = 1.00  (full)      — hard token, full acc.
 *
 * The thresholds are enum values so callers that want different
 * cutoffs can re-configure via ``cos_sigma_moe_set_cutoffs`` (kept
 * separate from the default for observability and v0 pinning).
 *
 * For top-K routing the same gate applies: ``top_k_route`` returns
 * K distinct experts, each with its own (width, width_frac) pair.
 *
 * Contracts (v0):
 *   1. route(σ, logits, N) returns expert_id = argmax(logits) when
 *      logits are finite and distinct.  Tie-break: lowest index.
 *   2. Width is monotonically non-decreasing in σ.
 *   3. NaN σ → width = FULL (safe default: run everything).
 *   4. NaN / -inf logits are skipped in argmax; all-NaN → expert_id
 *      = 0, score = NaN.
 *   5. top_k_route returns exactly K distinct expert_ids in
 *      descending-score order (no duplicates; K ≤ N).
 *   6. compute_saved() returns mean (1.0 − width_frac) — the fraction
 *      of FLOPs avoided across the activation log.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_MOE_H
#define COS_SIGMA_MOE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_MOE_WIDTH_Q  = 0,   /* 0.25  — quarter         */
    COS_MOE_WIDTH_H  = 1,   /* 0.50  — half            */
    COS_MOE_WIDTH_TQ = 2,   /* 0.75  — three-quarter   */
    COS_MOE_WIDTH_F  = 3,   /* 1.00  — full            */
} cos_moe_width_t;

typedef struct {
    int             expert_id;
    cos_moe_width_t width;
    float           width_frac;  /* 0.25 / 0.50 / 0.75 / 1.00 */
    float           score;       /* router_logits[expert_id]  */
} cos_moe_activation_t;

/* Width cutoffs in ascending order.  Default (MoSE):
 *     c_Q = 0.15   c_H = 0.30   c_TQ = 0.50
 * σ < c_Q → Q ; σ < c_H → H ; σ < c_TQ → TQ ; else F. */
typedef struct {
    float c_quarter;
    float c_half;
    float c_three_quarter;
} cos_moe_cutoffs_t;

extern const cos_moe_cutoffs_t COS_MOE_CUTOFFS_DEFAULT;

/* Human-readable label ("Q" | "H" | "TQ" | "F"). */
const char *cos_sigma_moe_width_label(cos_moe_width_t w);

/* Materialise a width enum into its fractional multiplier. */
float cos_sigma_moe_width_to_frac(cos_moe_width_t w);

/* σ-gated top-1 routing.
 *
 * Returns an activation with expert_id = argmax(router_logits[0..N])
 * (ties broken by lowest index) and width gated by σ against the
 * given cutoffs.  ``cutoffs`` may be NULL to use the default.
 *
 * Errors: if num_experts <= 0, returns {expert_id=-1, width=F,
 * width_frac=1.0, score=NaN}. */
cos_moe_activation_t cos_sigma_moe_route(
    float sigma,
    const float *router_logits, int num_experts,
    const cos_moe_cutoffs_t *cutoffs);

/* σ-gated top-K routing.
 *
 * Fills ``out`` with K activations (k ≤ num_experts), in descending
 * score order, no duplicates.  All K share the same σ-gated width.
 * Returns the number of activations actually filled (== k on
 * success; negative on argument violation). */
int cos_sigma_moe_top_k_route(
    float sigma,
    const float *router_logits, int num_experts,
    int k,
    const cos_moe_cutoffs_t *cutoffs,
    cos_moe_activation_t *out);

/* Compute-saved across an activation log.
 *
 * Returns the mean (1.0 − width_frac) over ``n`` activations; 0.0 if
 * n <= 0.  A value of 0.75 means 75% of FFN FLOPs were skipped on
 * average. */
float cos_sigma_moe_compute_saved(const cos_moe_activation_t *acts,
                                  int n);

/* Canonical self-test. */
int cos_sigma_moe_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_MOE_H */
