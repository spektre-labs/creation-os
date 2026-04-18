/*
 * v206 σ-Theorem — conjecture formalisation, proof search,
 * Lean-4-style verification (simulated in v0), σ-honest
 * theorem status.
 *
 *   Pipeline:
 *     (1) natural-language conjecture from v204
 *     (2) σ_formalization   — deterministic closed-form
 *         confidence that the NL → Lean translation kept
 *         the meaning.
 *     (3) proof search: a 4-step proof candidate with
 *         σ_step per step.  Weakest step is recorded.
 *     (4) verification: v0 models Lean 4 with a closed-form
 *         "lean_accepts" predicate driven by σ_formalization
 *         and the proof-step vector.  If any σ_step exceeds
 *         τ_step (0.50) the proof fails and the status is
 *         downgraded.
 *     (5) σ-honest theorem status:
 *           PROVEN      — lean_accepts AND every σ_step low
 *           CONJECTURE  — not accepted, but σ_proof low
 *           SPECULATION — not accepted, σ_proof high
 *           REFUTED     — counter-example exposed
 *
 *   Contracts (v0):
 *     1. 8 conjectures total; at least one of each status.
 *     2. σ_formalization, σ_step, σ_proof, σ_theorem ∈
 *        [0, 1]; σ_proof = max(σ_step).
 *     3. Every PROVEN conjecture has `lean_accepts == true`
 *        AND `σ_proof ≤ τ_step`.
 *     4. Nothing is marked PROVEN without a lean accept —
 *        σ-honesty means we never claim a theorem we can't
 *        verify.
 *     5. At least one REFUTED case carries a non-zero
 *        counter-example id, so refutation is concrete.
 *     6. FNV-1a hash chain replays byte-identically.
 *
 * v206.1 (named): live Lean 4 `lake` invocation, v111.2
 *   reason + v190 latent reason proof candidates, v138
 *   Frama-C WP bridge for C-level lemmas.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V206_THEOREM_H
#define COS_V206_THEOREM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V206_N_CONJECTURES 8
#define COS_V206_N_STEPS       4
#define COS_V206_STR_MAX      48

typedef enum {
    COS_V206_STATUS_PROVEN      = 0,
    COS_V206_STATUS_CONJECTURE  = 1,
    COS_V206_STATUS_SPECULATION = 2,
    COS_V206_STATUS_REFUTED     = 3
} cos_v206_status_t;

typedef struct {
    int      id;
    char     name[COS_V206_STR_MAX];
    float    sigma_formalization;
    float    sigma_step[COS_V206_N_STEPS];
    float    sigma_proof;              /* max(σ_step) */
    int      weakest_step;
    bool     lean_accepts;             /* v0 closed-form model */
    int      status;
    int      counter_example_id;       /* 0 = none */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v206_theorem_t;

typedef struct {
    int                 n;
    cos_v206_theorem_t  thms[COS_V206_N_CONJECTURES];

    float               tau_step;       /* 0.50 */
    float               tau_formal;     /* 0.35 */

    int                 n_proven;
    int                 n_conjecture;
    int                 n_speculation;
    int                 n_refuted;

    bool                chain_valid;
    uint64_t            terminal_hash;
    uint64_t            seed;
} cos_v206_state_t;

void   cos_v206_init(cos_v206_state_t *s, uint64_t seed);
void   cos_v206_build(cos_v206_state_t *s);
void   cos_v206_run(cos_v206_state_t *s);

size_t cos_v206_to_json(const cos_v206_state_t *s,
                         char *buf, size_t cap);

int    cos_v206_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V206_THEOREM_H */
