/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v189 σ-TTC — test-time compute allocated by σ.
 *
 *   Test-time scaling (Snell et al., 2024; Brown et al., 2024)
 *   says "think longer, answer better".  Nobody says HOW MUCH
 *   longer on which query.  v189 uses σ as the allocator:
 *
 *       σ_product first-token < 0.20        ⇒ 1 forward pass
 *       0.20 ≤ σ_product < 0.50              ⇒ 3 thinking paths
 *                                              + pick best
 *       σ_product ≥ 0.50                     ⇒ 8 thinking paths
 *                                              + v150 debate
 *                                              + v135 symbolic
 *                                              + v147 reflect
 *
 *   Per-token recurrent depth:
 *       low  σ-token : 1 layer iteration
 *       high σ-token : up to max_depth = 8 iterations of the
 *                      same block, early-stopping when
 *                      σ drops below τ_stop.
 *
 *   Compute-optimal budget:
 *       a fixed total budget `T_total` (e.g. 128 steps) is
 *       distributed across tokens by σ, so hard tokens get
 *       more steps than easy ones; uniform-best-of-N wastes
 *       the budget on easy tokens.
 *
 *   Exit invariants (exercised in the merge-gate):
 *
 *     1. Easy (σ<0.20) → exactly 1 path, ≤ 1 recurrent iter.
 *     2. Hard (σ≥0.50) → ≥ 8 paths *and* strictly > 4×
 *        more total compute than an easy query.
 *     3. σ-budget spending is monotone: mean compute per hard
 *        query ≥ 2× mean compute per medium query ≥ 2× mean
 *        compute per easy query.
 *     4. `[ttc] mode = "fast"` caps paths to 1, "balanced" to
 *        3, "deep" to ≥ 8 even if σ is small.
 *     5. Deterministic; byte-identical output on a fixed seed.
 *
 * v189.0 (this file) ships a deterministic fixture: 48 queries
 * × three σ-classes × three modes.  σ → budget mapping is
 * closed-form; no real forward passes.
 *
 * v189.1 (named, not shipped):
 *   - real v111.2 thinking path enumeration over BitNet-2B;
 *   - real per-layer recurrent depth with a halt predictor;
 *   - live `[ttc]` mode wired into /v1/chat/completions.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V189_TTC_H
#define COS_V189_TTC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V189_N_QUERIES      48
#define COS_V189_MAX_TOKENS     16
#define COS_V189_N_MODES         3
#define COS_V189_STR_MAX        32

typedef enum {
    COS_V189_MODE_FAST     = 0,
    COS_V189_MODE_BALANCED = 1,
    COS_V189_MODE_DEEP     = 2
} cos_v189_mode_t;

typedef enum {
    COS_V189_CLASS_EASY   = 0,
    COS_V189_CLASS_MEDIUM = 1,
    COS_V189_CLASS_HARD   = 2
} cos_v189_class_t;

typedef struct {
    int    id;
    int    class_;                  /* cos_v189_class_t */
    float  sigma_first_token;
    int    n_tokens;
    float  sigma_token[COS_V189_MAX_TOKENS];

    int    depth_token[COS_V189_MAX_TOKENS]; /* recurrent iterations */
    int    n_paths;                 /* thinking paths explored */
    bool   used_debate;             /* v150 */
    bool   used_symbolic;           /* v135 */
    bool   used_reflect;            /* v147 */
    int    compute_units;           /* total compute spent */
} cos_v189_query_t;

typedef struct {
    int                mode;                 /* cos_v189_mode_t */
    float              tau_easy;             /* < τ_easy ⇒ easy */
    float              tau_hard;             /* ≥ τ_hard ⇒ hard */
    float              tau_stop;             /* recurrent early stop */
    int                max_depth;
    int                max_paths_deep;
    int                budget_total;

    int                n_queries;
    cos_v189_query_t   queries[COS_V189_N_QUERIES];

    /* Per-class aggregates. */
    int                n_easy, n_medium, n_hard;
    double             sum_compute_easy;
    double             sum_compute_medium;
    double             sum_compute_hard;
    double             mean_compute_easy;
    double             mean_compute_medium;
    double             mean_compute_hard;
    float              ratio_hard_over_easy;   /* ≥ 4.0 contract */

    uint64_t           seed;
} cos_v189_state_t;

void   cos_v189_init  (cos_v189_state_t *s, uint64_t seed, int mode);
void   cos_v189_build (cos_v189_state_t *s);
void   cos_v189_run   (cos_v189_state_t *s);

size_t cos_v189_to_json(const cos_v189_state_t *s,
                         char *buf, size_t cap);

int    cos_v189_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V189_TTC_H */
