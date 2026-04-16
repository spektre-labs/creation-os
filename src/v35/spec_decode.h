/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v35: σ-guided speculative decoding (lab; not merge-gate).
 *
 * Adaptive draft length uses a monotone [0,1) score
 *   u = σ_epistemic / (1 + σ_epistemic)
 * so fixed thresholds (0.7, 0.4, 0.2) apply regardless of raw Dirichlet scale.
 */
#ifndef CREATION_OS_V35_SPEC_DECODE_H
#define CREATION_OS_V35_SPEC_DECODE_H

#include "../sigma/decompose.h"
#include "../v33/model_registry.h"

typedef struct {
    const cos_model_entry_t *draft;
    const cos_model_entry_t *verifier;
    /** Legacy single knob (optional); prefer routing thresholds below. */
    float sigma_threshold;
    int max_draft_tokens;
} cos_spec_config_t;

typedef struct {
    float sigma_local_threshold;
    float sigma_api_threshold;
    float verifier_abstain_epistemic;
    char draft_name[64];
    char verifier_name[64];
} cos_spec_routing_t;

typedef enum {
    COS_SPEC_TIER_LOCAL_ONLY = 0,
    COS_SPEC_TIER_SPEC_LOCAL = 1,
    COS_SPEC_TIER_API = 2
} cos_spec_tier_t;

void cos_spec_routing_defaults(cos_spec_routing_t *r);
int cos_spec_routing_load(const char *path, cos_spec_routing_t *r);

/** u = epistemic / (1+epistemic) */
float cos_spec_epistemic_unit_score(const sigma_decomposed_t *s);

/**
 * Adaptive K from epistemic score (v35 baseline policy).
 * Capped by max_k (caller supplies cfg.max_draft_tokens).
 */
int cos_spec_compute_draft_length(sigma_decomposed_t s, int max_k);

cos_spec_tier_t cos_spec_route_tier(float u, const cos_spec_routing_t *r);

typedef struct {
    int accepted;
    int rejected;
    int abstained;
} cos_spec_verify_stats_t;

/**
 * Dual-σ policy: if draft agrees, still abstain when verifier epistemic (raw) is high.
 * verifier_epistemic_per_token should be precomputed from verifier logits per step.
 */
void cos_spec_verify_with_dual_sigma(const int *draft_agrees,
    const float *verifier_epistemic_per_token,
    int n,
    float abstain_threshold,
    cos_spec_verify_stats_t *out);

#endif /* CREATION_OS_V35_SPEC_DECODE_H */
