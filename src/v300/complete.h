/*
 * v300 σ-Complete — the 300th layer is the audit of the
 * other 299.
 *
 *   Creation OS now has 300 layers.  v300 does not add a
 *   capability.  It asks, byte-deterministically, "is the
 *   architecture complete, or is it just big?"  The v0
 *   manifest fires four audits:
 *
 *   (1) Cognitive completeness audit.  v243 enumerates
 *   a 15-category cognitive taxonomy (perception,
 *   memory, reasoning, planning, learning, language,
 *   action, metacognition, emotion, social, ethics,
 *   creativity, self_model, embodiment, consciousness).
 *   v300 asserts every category has ≥ 1 representative
 *   kernel in v6..v300 and every category has
 *   `covered = true`.
 *
 *   (2) Dependency graph.  v300 classifies the 300
 *   kernels into three buckets — `core_critical`
 *   (the 7 pyramid invariants that must not be
 *   removed), `supporting` (the rest), and
 *   `removable_duplicate` (must be 0 after v300's
 *   audit).  Contract: the three counts sum to 300,
 *   `removable_duplicate == 0`, `core_critical == 7`.
 *
 *   (3) 1 = 1 self-test.  v106.1 asked this question
 *   for one kernel; v300 asks it for the whole repo.
 *   Four canonical repo-level claims (`zero_deps`,
 *   `sigma_gated`, `deterministic`, `monotonic_clock`)
 *   each carry `declared_in_readme` AND
 *   `realized_in_code`.  `sigma_pair = 0` iff declared
 *   equals realized.  `sigma_repo` is the mean.
 *   v0 requires every pair aligned AND `sigma_repo <
 *   τ_repo = 0.10`.
 *
 *   (4) Pyramid test (will-it-survive-100-years).  The
 *   7 invariants the "immortal" layer (v287..v298) was
 *   written to enforce: v287 granite zero_deps, v288
 *   oculus tunable_aperture, v289 ruin_value graceful
 *   degradation, v290 dougong modular_coupling, v293
 *   hagia_sofia continuous_use, v297 clock
 *   time_invariant, v298 rosetta self_documenting.
 *   Every row must have `invariant_holds = true`;
 *   `architecture_survives_100yr = (all 7 hold)`.
 *
 *   σ_complete (surface hygiene):
 *      σ_complete = 1 − (
 *          cog_rows_ok + cog_covered_ok +
 *          dep_sum_ok + dep_removable_zero_ok +
 *            dep_critical_count_ok +
 *          repo_rows_ok + repo_declared_eq_realized_ok +
 *            repo_sigma_under_threshold_ok +
 *          pyr_rows_ok + pyr_all_hold_ok +
 *            pyr_survives_ok
 *      ) / (
 *          15 + 1 + 1 + 1 + 1 + 4 + 1 + 1 + 7 + 1 + 1
 *      )
 *   v0 requires `σ_complete == 0.0`.
 *
 *   Contracts (v0):
 *     1. 15 cognitive categories in canonical order
 *        (v243 list); each `covered = true` with a
 *        named representative_kernel; every kernel
 *        lies in the v6..v300 range.
 *     2. Dependency graph: `kernels_total = 300`;
 *        `core_critical + supporting +
 *          removable_duplicate == kernels_total`;
 *        `removable_duplicate == 0`;
 *        `core_critical == 7`.
 *     3. 4 repo-level 1=1 claims; each row `declared`
 *        equals `realized`; `sigma_pair == 0`;
 *        `sigma_repo == 0 < τ_repo = 0.10`.
 *     4. 7 pyramid invariants in canonical order; each
 *        row `invariant_holds = true`;
 *        `architecture_survives_100yr = true`.
 *     5. σ_complete ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v300.1 (named, not implemented): a nightly job that
 *     regenerates `dependency_graph.dot` from the live
 *     Makefile and fails merge-gate if the observed
 *     critical path drifts from the v300 v0 manifest;
 *     a README-to-code drift detector that recomputes
 *     `sigma_repo` from actual README bullet points and
 *     kernel self-tests rather than the hand-curated
 *     v0 table; a 100-year checkpoint that re-emits
 *     the pyramid test from an archival tarball whose
 *     only dependency is a C11 compiler and `bash`.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V300_COMPLETE_H
#define COS_V300_COMPLETE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V300_N_COG      15
#define COS_V300_N_DEP      3
#define COS_V300_N_REPO     4
#define COS_V300_N_PYR      7

#define COS_V300_KERNELS_TOTAL     300
#define COS_V300_CORE_CRITICAL_N   7
#define COS_V300_TAU_REPO          0.10f

typedef struct {
    char category            [20];
    int  representative_kernel;
    bool covered;
} cos_v300_cog_t;

typedef struct {
    char bucket[24];
    int  count;
} cos_v300_dep_t;

typedef struct {
    char claim[22];
    bool declared_in_readme;
    bool realized_in_code;
    float sigma_pair;
} cos_v300_repo_t;

typedef struct {
    char kernel_label[18];
    char invariant    [22];
    bool invariant_holds;
} cos_v300_pyr_t;

typedef struct {
    cos_v300_cog_t  cog [COS_V300_N_COG];
    cos_v300_dep_t  dep [COS_V300_N_DEP];
    cos_v300_repo_t repo[COS_V300_N_REPO];
    cos_v300_pyr_t  pyr [COS_V300_N_PYR];

    int  n_cog_rows_ok;
    bool cog_covered_ok;

    int  kernels_total;
    bool dep_sum_ok;
    bool dep_removable_zero_ok;
    bool dep_critical_count_ok;

    int  n_repo_rows_ok;
    bool repo_declared_eq_realized_ok;
    float sigma_repo;
    bool repo_sigma_under_threshold_ok;

    int  n_pyr_rows_ok;
    bool pyr_all_hold_ok;
    bool architecture_survives_100yr;

    float sigma_complete;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v300_state_t;

void   cos_v300_init(cos_v300_state_t *s, uint64_t seed);
void   cos_v300_run (cos_v300_state_t *s);

size_t cos_v300_to_json(const cos_v300_state_t *s,
                         char *buf, size_t cap);

int    cos_v300_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V300_COMPLETE_H */
