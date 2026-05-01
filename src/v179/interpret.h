/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v179 σ-Interpret — mechanistic interpretability for σ.
 *
 *   The field attempts to understand *what* the model is
 *   doing inside.  Creation OS already has σ measuring
 *   uncertainty *from the outside*.  v179 joins them:
 *
 *       σ-signal  ─►  SAE decomposition      ─►  offending feature
 *                 ─►  σ-circuit back-trace    ─►  head + MLP + token
 *                 ─►  v140 causal attribution ─►  responsible channel
 *                 ─►  human-readable reason
 *
 *   The exit invariants (exercised in the merge-gate):
 *
 *     1. SAE finds ≥ 5 features correlated with σ_product
 *        above |r| ≥ 0.60.
 *     2. The top-correlated feature carries a human-readable
 *        label describing the uncertainty mode it captures.
 *     3. A σ-rise at a given token is traced back to exactly
 *        one attention head + one MLP neuron + one input
 *        token — the circuit chain is non-empty and each
 *        link is addressable.
 *     4. The emitted explanation mentions the token, the
 *        SAE feature id and label, the σ value, and the
 *        collapsed `n_effective` channel delta.
 *
 * v179.0 (this file) ships a deterministic, weights-free
 * fixture: 64 synthetic prompts × 8 attention heads × 8 MLP
 * neurons × 24 SAE features at layer 15.  Every σ, every
 * activation, every correlation is closed-form.
 *
 * v179.1 (named, not shipped):
 *   - real SAE weights over BitNet-2B layer 15 with 2 560 →
 *     8 192 feature dictionary
 *   - real `GET /v1/explain/{response_id}` HTTP endpoint
 *     with the web-UI σ-timeline
 *   - real v140 causal-channel wiring
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V179_INTERPRET_H
#define COS_V179_INTERPRET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V179_N_PROMPTS      64
#define COS_V179_N_FEATURES     24
#define COS_V179_N_HEADS         8
#define COS_V179_N_MLP           8
#define COS_V179_MAX_STR        96

typedef struct {
    int   id;
    char  prompt[COS_V179_MAX_STR];
    float sigma_product;                          /* target */
    float features[COS_V179_N_FEATURES];          /* SAE activations */
    int   top_token;                              /* input token index */
    int   top_head;                               /* attention head id */
    int   top_mlp;                                /* MLP neuron id */
    int   n_eff_before;                           /* v140 channel width */
    int   n_eff_after;
} cos_v179_sample_t;

typedef struct {
    int   feature_id;
    char  label[COS_V179_MAX_STR];
    float correlation;                            /* Pearson r with σ */
    float mean_when_high_sigma;                   /* diagnostic */
    float mean_when_low_sigma;
} cos_v179_feature_t;

typedef struct {
    int    sample_id;
    int    feature_id;
    float  sigma_product;
    int    token_pos;
    int    head_id;
    int    mlp_id;
    int    n_eff_before;
    int    n_eff_after;
    char   explanation[COS_V179_MAX_STR * 3];
} cos_v179_explain_t;

typedef struct {
    int                n_samples;
    cos_v179_sample_t  samples[COS_V179_N_PROMPTS];

    int                n_features_correlated;
    cos_v179_feature_t features[COS_V179_N_FEATURES];

    float              tau_correlated;      /* |r| ≥ this ⇒ "correlated" */
    float              tau_high_sigma;      /* defines "high σ" bucket */
    uint64_t           seed;
} cos_v179_state_t;

void cos_v179_init(cos_v179_state_t *s, uint64_t seed);
void cos_v179_generate_samples(cos_v179_state_t *s);
void cos_v179_fit_sae(cos_v179_state_t *s);

int  cos_v179_explain(const cos_v179_state_t *s,
                      int sample_id,
                      cos_v179_explain_t *out);

size_t cos_v179_to_json     (const cos_v179_state_t *s,
                              char *buf, size_t cap);
size_t cos_v179_explain_json(const cos_v179_explain_t *e,
                              char *buf, size_t cap);

int cos_v179_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V179_INTERPRET_H */
