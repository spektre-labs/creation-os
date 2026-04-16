/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v33/v34: σ-routed SLM-default / LLM-fallback router (lab; not merge-gate).
 */
#ifndef CREATION_OS_V33_ROUTER_H
#define CREATION_OS_V33_ROUTER_H

typedef enum {
    COS_ROUTE_PRIMARY = 0,
    COS_ROUTE_FALLBACK = 1,
    COS_ROUTE_ABSTAIN = 2,
    /** High aleatoric only: emit normally but tag “multiple valid interpretations” (v34). */
    COS_ROUTE_PRIMARY_AMBIGUOUS = 3
} cos_route_decision_t;

typedef struct {
    float threshold_logit_entropy;
    float threshold_top_margin;
    char fallback_model_name[128];
    char fallback_endpoint[512];
    int max_fallback_budget_per_session;
    /** 0 = legacy entropy+margin; 1 = v34 decomposed + optional Platt. */
    int routing_mode_v34;
    float threshold_aleatoric;
    /** Used when Platt params are valid: abstain/fallback if P(correct) < this. */
    float threshold_min_p_correct;
    /** Used when Platt is not valid: compare raw epistemic scalar. */
    float threshold_epistemic_raw;
    float platt_a;
    float platt_b;
    int platt_calib_valid;
    char sigma_calibration_path[256];
} cos_routing_config_t;

typedef struct {
    cos_route_decision_t decision;
    /** Bitmask: 1 = logit_entropy trip, 2 = top_margin trip (legacy); 4 = aleatoric ambiguity (v34). */
    unsigned abstain_channel_mask;
    int budget_exhausted;
} cos_route_result_t;

void cos_routing_defaults(cos_routing_config_t *cfg);
/** Load JSON from path; missing keys keep defaults. Returns 0 on success, -1 I/O or parse failure. */
int cos_routing_load(const char *path, cos_routing_config_t *cfg);

/**
 * Legacy route from raw logits using σ_logit_entropy and σ_top_margin (src/sigma/channels.c).
 * If decision is FALLBACK, decrements *fallback_budget_remaining_io when non-NULL.
 */
cos_route_result_t cos_route_from_logits(const cos_routing_config_t *cfg,
    const float *logits,
    int n_logits,
    int fallback_model_available,
    int *fallback_budget_remaining_io);

/**
 * v34: Dirichlet-style epistemic/aleatoric split + optional Platt calibration on epistemic raw,
 * then top-margin as an auxiliary trip (same as legacy margin bit).
 */
cos_route_result_t cos_route_from_logits_v34(const cos_routing_config_t *cfg,
    const float *logits,
    int n_logits,
    int fallback_model_available,
    int *fallback_budget_remaining_io);

#endif /* CREATION_OS_V33_ROUTER_H */
