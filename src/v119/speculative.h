/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v119 σ-Speculative — draft-verify decoding with σ-adaptive γ.
 *
 * Standard speculative decoding (Leviathan 2023, Chen 2023): a small
 * draft model generates γ tokens ahead, a large target model verifies
 * them in a single forward pass, tokens are accepted under a
 * probability ratio criterion p_target(tok) / p_draft(tok).  Modern
 * implementations (vLLM, llama.cpp, gpt-oss) use a fixed γ.
 *
 * v119 adds two σ-governance hooks that nobody else ships today:
 *
 *   1. Adaptive γ from draft σ_product:
 *          γ = clamp(γ_min, γ_max,
 *                    round(γ_base · (1 − σ_product_draft)))
 *      When the draft is confident (σ small), we commit to a longer
 *      look-ahead.  When the draft is uncertain, we shrink γ so we
 *      don't waste the target model on tokens likely to be rejected
 *      anyway.
 *
 *   2. σ-gated verification: a draft token is only *considered for
 *      acceptance* when its σ_product is below τ_spec.  Above τ_spec
 *      the token is auto-rejected and the target model takes over
 *      from that offset — saving the target's verification bandwidth
 *      for the tokens the draft is confident about.
 *
 * Everything here is the **policy**: given a draft candidate (token
 * + σ + model probability) and the target's verification output
 * (p_target), decide accept / reject / auto-reject.  The actual model
 * forward passes live in the v101 bridge and are plumbed in at
 * v119.1 — see docs/v119/README.md.  v119.0 self-tests and smoke
 * harness use deterministic synthetic streams so the merge-gate
 * stays weights-free.
 */
#ifndef COS_V119_SPECULATIVE_H
#define COS_V119_SPECULATIVE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V119_GAMMA_BASE_DEFAULT   8
#define COS_V119_GAMMA_MIN_DEFAULT    2
#define COS_V119_GAMMA_MAX_DEFAULT   12
#define COS_V119_TAU_SPEC_DEFAULT    0.70f

typedef enum {
    COS_V119_DECISION_ACCEPT      = 0,  /* draft token kept as-is */
    COS_V119_DECISION_REJECT      = 1,  /* target disagrees — fall back */
    COS_V119_DECISION_SIGMA_GATE  = 2,  /* σ > τ → auto-reject */
    COS_V119_DECISION_SAMPLED     = 3,  /* target sampled its own token after mismatch */
} cos_v119_decision_t;

typedef struct cos_v119_draft_token {
    int32_t token_id;
    float   p_draft;         /* draft model probability for token_id */
    float   sigma_product;   /* σ_product observed at this draft step */
} cos_v119_draft_token_t;

typedef struct cos_v119_verify_token {
    int32_t token_id;        /* argmax(p_target) — target's own choice */
    float   p_target;        /* target model probability at that position
                                on the *draft* token_id */
} cos_v119_verify_token_t;

typedef struct cos_v119_config {
    int   gamma_base;        /* 8 */
    int   gamma_min;         /* 2 */
    int   gamma_max;         /* 12 */
    float tau_spec;          /* 0.70 */
    /* p_target(draft_tok) ≥ p_draft(draft_tok) * accept_ratio keeps
     * the draft token; smaller accept_ratio = more permissive.       */
    float accept_ratio;      /* default 1.0 */
} cos_v119_config_t;

typedef struct cos_v119_step_record {
    cos_v119_decision_t decision;
    int32_t token_id;            /* the token that actually advanced */
    float   sigma_product;       /* σ on the draft step */
    float   p_draft;
    float   p_target;            /* 0 on σ-gate (no target consulted) */
} cos_v119_step_record_t;

typedef struct cos_v119_stats {
    int tokens_emitted;          /* draft accepts + fallback samples */
    int tokens_accepted;         /* DECISION_ACCEPT count */
    int tokens_rejected;         /* DECISION_REJECT count */
    int tokens_sigma_gated;      /* DECISION_SIGMA_GATE count */
    int tokens_sampled;          /* DECISION_SAMPLED count */
    int target_invocations;      /* how many draft tokens reached the target */
    int gammas_adapted_down;     /* step-group size reduced by σ */
    int gammas_adapted_up;       /* step-group size increased by σ */
    double sum_sigma_draft;      /* running sum of per-step σ */
    double sum_gamma;            /* running sum of adapted γ */
    int    groups;               /* # of γ-sized draft groups processed */
} cos_v119_stats_t;

/* Fill defaults (γ_base=8, γ_min=2, γ_max=12, τ_spec=0.70). */
void cos_v119_config_defaults(cos_v119_config_t *cfg);

/* Compute the σ-adaptive γ for the NEXT draft window.  Given the
 * most recent observed σ_product from the draft (0 at the very start,
 * use γ_base), returns γ clamped to [γ_min, γ_max].  Deterministic. */
int  cos_v119_adaptive_gamma(const cos_v119_config_t *cfg,
                             float recent_sigma);

/* Verify one draft group of length `n_draft` against the matching
 * target outputs.  Writes up to `n_draft` step records into `out`;
 * returns the number of records written, which equals the tokens
 * actually advanced (accepts + at most one fallback sample).
 *
 * Semantics:
 *   - For each i in [0, n_draft):
 *       if σ_i > τ_spec                   → SIGMA_GATE, stop the group
 *       else if target argmax == draft_i  AND
 *               p_target_i ≥ accept_ratio · p_draft_i
 *                                         → ACCEPT, continue
 *       else                              → REJECT + one SAMPLED fallback,
 *                                            stop the group
 *
 * `verify` is the target output for each draft position; when the
 * draft token was σ-gated the target need not have been invoked —
 * callers may pass a zero-initialised verify entry. */
int  cos_v119_verify_group(const cos_v119_config_t *cfg,
                           const cos_v119_draft_token_t *draft, int n_draft,
                           const cos_v119_verify_token_t *verify,
                           cos_v119_step_record_t *out);

/* High-level simulator: runs the adaptive γ + verify loop over a
 * fixed-length synthetic stream and updates `stats`.  Used by CI to
 * measure acceptance rate + target-call savings in the weights-free
 * path.
 *
 * `stream_len` is the total number of draft candidate tokens
 * available; `drafts` / `verifies` arrays must each have that many
 * entries. */
void cos_v119_simulate(const cos_v119_config_t *cfg,
                       const cos_v119_draft_token_t *drafts,
                       const cos_v119_verify_token_t *verifies,
                       int stream_len,
                       cos_v119_stats_t *stats);

/* Summary ratios (pure derivations over `stats`). */
float cos_v119_acceptance_rate (const cos_v119_stats_t *s);
float cos_v119_target_save_rate(const cos_v119_stats_t *s, int stream_len);
float cos_v119_mean_gamma      (const cos_v119_stats_t *s);

/* Serialise stats + derived ratios to a single JSON object. */
int   cos_v119_stats_to_json(const cos_v119_stats_t *s, int stream_len,
                             char *out, size_t cap);

/* Pure-C self-test.  Verifies:
 *   - defaults
 *   - adaptive γ: low σ → γ_max, high σ → γ_min, monotonic
 *   - σ-gate aborts a group early and bumps target_save
 *   - probability-ratio accept + target-argmax mismatch reject
 *   - fallback sample on reject, stats accounting consistent
 *   - acceptance rate on synthetic streams (confident vs uncertain)
 * Returns 0 on pass. */
int  cos_v119_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_V119_SPECULATIVE_H */
