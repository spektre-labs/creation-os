/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v226 σ-Attention — σ-interpretation of attention.
 *
 *   Q = observer,  K = observed,  V = meaning,
 *   softmax(QK^T / √d) = σ-distribution over keys.
 *
 *   Shannon entropy of that distribution is the
 *   per-(head, token) σ:
 *       σ_head_token = H / log(L)           ∈ [0, 1]
 *   where L is the key-length.  A sharp softmax (H≈0)
 *   ⇒ σ≈0 (the head knows exactly where to look), a
 *   flat softmax (H≈log L) ⇒ σ≈1 (the head looks
 *   everywhere → indecisive).
 *
 *   Per-head σ (v0):
 *       σ_head = mean_t σ_head_token
 *   Heads below τ_useful (0.40) are 'confident';
 *   heads above τ_noisy  (0.70) are flagged for
 *   prune / surgery by v177 / v180 (names only).
 *
 *   Attention surgery (v0 as a classification):
 *       action = PRUNE    if σ_head > τ_noisy
 *       action = BOOST    if σ_head < τ_useful
 *       action = KEEP     otherwise
 *   Counts reported but no weights are actually
 *   modified (v0 contract: offline, weights-free).
 *
 *   Fixture (deterministic, 2 layers × 4 heads = 8
 *   heads, 6 tokens, key-length 6).  Heads are
 *   constructed with three σ-profiles:
 *       'factual heads' (heads 1, 6): very sharp
 *       'diffuse heads' (heads 3, 5): near-uniform
 *       'mixed heads'   (heads 0, 2, 4, 7): in between
 *   so the contract has something concrete to verify.
 *
 *   Contracts (v0):
 *     1. σ_head_token, σ_head ∈ [0, 1].
 *     2. ≥ 1 head with σ_head < τ_useful ('valuable').
 *     3. ≥ 1 head with σ_head > τ_noisy  ('flagged').
 *     4. Softmax rows sum to 1 ± 1e-5 (numerical
 *        contract).
 *     5. Max σ_head − Min σ_head ≥ 0.30 (heads are
 *        actually differentiated, not all the same).
 *     6. FNV-1a chain over Q·K^T, softmax, σ per
 *        (head, token), and summary replays byte-
 *        identically.
 *
 *   v226.1 (named): live transformer attention export,
 *     v180 attention surgery wiring, web /attention
 *     dashboard, causal per-head σ ablation, multi-
 *     layer × multi-head tree aggregation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V226_ATTENTION_H
#define COS_V226_ATTENTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V226_N_HEADS   8
#define COS_V226_N_TOKENS  6
#define COS_V226_KEY_LEN   6

typedef enum {
    COS_V226_ACTION_KEEP  = 0,
    COS_V226_ACTION_BOOST = 1,
    COS_V226_ACTION_PRUNE = 2
} cos_v226_action_t;

typedef struct {
    int      head_id;
    float    temperature;                            /* sharpness */
    float    softmax [COS_V226_N_TOKENS][COS_V226_KEY_LEN];
    float    qk      [COS_V226_N_TOKENS][COS_V226_KEY_LEN];
    float    sigma_token[COS_V226_N_TOKENS];          /* σ_head_token */
    float    sigma_head;                              /* mean σ_token */
    int      action;                                  /* cos_v226_action_t */
} cos_v226_head_t;

typedef struct {
    cos_v226_head_t heads[COS_V226_N_HEADS];

    float    tau_useful;      /* 0.40 */
    float    tau_noisy;       /* 0.70 */

    int      n_valuable;      /* σ_head < τ_useful */
    int      n_flagged;       /* σ_head > τ_noisy  */
    int      n_boost;
    int      n_prune;
    int      n_keep;

    float    sigma_head_min;
    float    sigma_head_max;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v226_state_t;

void   cos_v226_init(cos_v226_state_t *s, uint64_t seed);
void   cos_v226_build(cos_v226_state_t *s);
void   cos_v226_run(cos_v226_state_t *s);

size_t cos_v226_to_json(const cos_v226_state_t *s,
                         char *buf, size_t cap);

int    cos_v226_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V226_ATTENTION_H */
