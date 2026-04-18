/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v111.2 — σ-Reasoning Endpoint.
 *
 * Composes four existing Creation OS layers into a single inference
 * pipeline behind a new OpenAI-shaped HTTP endpoint (POST /v1/reason).
 * The wiring is:
 *
 *     v62  reasoning fabric     — Energy-Based path-consistency gate.
 *     v64  intellect / MCTS    — multi-candidate selection heuristic.
 *     v101 BitNet bridge       — actual token generation + σ profile.
 *     v45  introspection       — predicted-vs-observed waypoint check.
 *
 * High-level contract
 * -------------------
 *
 * Given a user prompt and k ∈ [1, 8] candidates, the pipeline:
 *   1. expands the prompt into k perturbed variants (prompt-stem
 *      permutations — deterministic, no temperature sampling required);
 *   2. runs v101 greedy generation per variant, collecting the full
 *      σ-profile and the scalar σ_product per candidate;
 *   3. selects the candidate with the lowest σ_product (most confident
 *      under the v105 default aggregator);
 *   4. applies an introspection consistency check: the predicted σ at
 *      selection time is compared with the σ observed on a short
 *      re-generation of the chosen candidate's own continuation.  If
 *      they disagree by more than waypoint_delta, the pipeline flags
 *      `waypoint_match = false`;
 *   5. abstains (returns empty text with `abstained = 1`) when either
 *        (a) σ_product of the chosen candidate > tau_abstain, or
 *        (b) the best-to-mean σ gap is below min_margin (the k-way
 *            vote is undifferentiated; σ carries no rank signal).
 *
 * Everything is deterministic given the bridge and the input.  No extra
 * model weights are loaded — this layer only composes existing APIs.
 */
#ifndef COS_V111_REASON_H
#define COS_V111_REASON_H

#include <stddef.h>
#include <stdint.h>
#include "../v101/bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { COS_V111_REASON_MAX_CANDIDATES = 8 };

typedef struct cos_v111_reason_candidate {
    char  text[4096];
    int   n_text;                    /* bytes written to text (no NUL) */
    float sigma_profile[COS_V101_SIGMA_CHANNELS];
    float sigma_mean;
    float sigma_product;
    int   abstained;                 /* 1 iff v101 aborted early */
    int   stem_id;                   /* which prompt-stem variant (0..k-1) */
} cos_v111_reason_candidate_t;

typedef struct cos_v111_reason_opts {
    int   k_candidates;              /* 1..8; default 3 */
    int   max_tokens;                /* default 128 */
    float tau_abstain;               /* default inherited from server cfg */
    float waypoint_delta;            /* σ disagreement tolerance; default 0.08 */
    float min_margin;                /* min(best_σ − mean_σ); default 0.005 */
} cos_v111_reason_opts_t;

typedef struct cos_v111_reason_report {
    cos_v111_reason_candidate_t candidates[COS_V111_REASON_MAX_CANDIDATES];
    int   n_candidates;
    int   chosen_index;              /* -1 if abstained */
    float chosen_sigma_product;
    float mean_sigma_product;        /* over all candidates, for diagnostics */
    float margin;                    /* mean − best (non-negative if healthy) */
    int   waypoint_match;            /* 1 iff introspection agrees */
    float waypoint_sigma_predicted;
    float waypoint_sigma_observed;
    int   abstained;                 /* final decision */
    const char *abstain_reason;      /* pointer to static string; NULL if ok */
} cos_v111_reason_report_t;

/* Pre-registered prompt-stem perturbations.  Deterministic and
 * reproducible.  Up to 8 stems — the k_candidates cap is 8 for this
 * reason.  Index 0 is the identity ("") so k=1 is a pure v101 generate. */
const char *cos_v111_reason_stem(int stem_id);

/* Zero-initialise opts with pipeline defaults. */
void cos_v111_reason_opts_defaults(cos_v111_reason_opts_t *o);

/* Run the full pipeline.  Bridge must be a loaded v101 bridge in real
 * mode.  Returns 0 on success (including graceful abstain), non-zero on
 * infrastructure error.  On success the report is fully populated. */
int cos_v111_reason_run(cos_v101_bridge_t *bridge,
                        const char *prompt,
                        const cos_v111_reason_opts_t *opts,
                        cos_v111_reason_report_t *out);

/* Serialise the report as JSON into `buf`.  Returns bytes written
 * (excluding NUL), or -1 on truncation. */
int cos_v111_reason_report_to_json(const cos_v111_reason_report_t *r,
                                   const char *model_id,
                                   char *buf, int cap);

/* Pure-C self-test that runs WITHOUT a loaded model: exercises the stem
 * registry, opts defaults, JSON serialiser on a synthetic report, and
 * the consistency-check arithmetic.  Prints a PASS/FAIL summary.  Used
 * by check-v111-reason. */
int cos_v111_reason_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_V111_REASON_H */
