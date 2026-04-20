/*
 * v289 σ-Ruin-Value — graceful degradation.
 *
 *   Ruin-value architecture (Speer): a building
 *   should fall apart beautifully — every stage of
 *   collapse still has standing structure.  v289
 *   types the same discipline for Creation OS: each
 *   kernel v6..v286 is independently viable, the
 *   fallback cascade has four tiers each standalone,
 *   σ-log is always recoverable, and 5 seed kernels
 *   (v229) are enough to regrow the whole stack from
 *   wreckage.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Kernel removals (exactly 4 canonical rows):
 *     Each: `kernel_removed`, `survivor`,
 *     `survivor_still_works == true`.
 *     Canonical: `v267_mamba` → `transformer`;
 *     `v260_engram` → `local_memory`;
 *     `v275_ttt` → `frozen_weights`;
 *     `v262_hybrid` → `direct_kernel`.
 *     Contract: 4 distinct removals; every survivor
 *     still works — removing any named subsystem
 *     does not take down the rest.
 *
 *   Fallback cascade (exactly 4 canonical tiers):
 *     tier 1: `hybrid_engine`        cost 1.00;
 *     tier 2: `transformer_only`     cost 0.60;
 *     tier 3: `bitnet_plus_sigma`    cost 0.25;
 *     tier 4: `pure_sigma_gate`      cost 0.05.
 *     Each: `tier_id ∈ {1..4}`, `name`,
 *     `resource_cost ∈ [0, 1]`,
 *     `standalone_viable == true`.
 *     Contract: `tier_id` permutation [1..4]; every
 *     tier standalone-viable; `resource_cost`
 *     strictly monotonically decreasing — the
 *     bottom tier (σ-gate alone) is the "still
 *     beautiful ruin" of last resort.
 *
 *   Data preservation (exactly 3 canonical
 *   guarantees):
 *     `sigma_log_persisted` ·
 *     `atomic_write_wal` ·
 *     `last_measurement_recoverable`, each
 *     `guaranteed == true`.
 *     Contract: 3 distinct names; every guarantee
 *     true — the last measurement survives any
 *     crash.
 *
 *   Rebuild from ruin (exactly 3 canonical steps
 *   with canonical order):
 *     step_order 1 → `read_sigma_log`;
 *     step_order 2 → `restore_last_state`;
 *     step_order 3 → `resume_not_restart`.
 *     Each: `step_name`, `step_order ∈ {1..3}`,
 *     `possible == true`.
 *     Contract: canonical names in canonical order
 *     (permutation [1, 2, 3]); every step
 *     possible — the system resumes instead of
 *     restarting.
 *
 *   Seed regrowth (scalar invariant): the v229 seed
 *   bundle requires ≤ 5 kernels to reconstitute the
 *   full stack.  The v0 field
 *   `seed_kernels_required` must satisfy
 *   `seed_kernels_required == 5`.
 *
 *   σ_ruin (surface hygiene):
 *       σ_ruin = 1 −
 *         (removal_rows_ok + removal_all_survive_ok +
 *          removal_distinct_ok + cascade_rows_ok +
 *          cascade_tier_permutation_ok +
 *          cascade_all_viable_ok +
 *          cascade_cost_monotone_ok +
 *          preservation_rows_ok +
 *          preservation_all_guaranteed_ok +
 *          rebuild_rows_ok + rebuild_order_ok +
 *          rebuild_all_possible_ok +
 *          seed_required_ok) /
 *         (4 + 1 + 1 + 4 + 1 + 1 + 1 + 3 + 1 + 3 + 1 + 1 + 1)
 *   v0 requires `σ_ruin == 0.0`.
 *
 *   Contracts (v0):
 *     1. 4 removal rows canonical; all survivors
 *        still work; all distinct.
 *     2. 4 cascade tiers canonical; `tier_id`
 *        permutation [1..4]; all standalone-viable;
 *        `resource_cost` strictly decreasing.
 *     3. 3 preservation rows canonical; all
 *        guaranteed.
 *     4. 3 rebuild steps canonical; step_order
 *        permutation [1, 2, 3]; all possible.
 *     5. `seed_kernels_required == 5`.
 *     6. σ_ruin ∈ [0, 1] AND == 0.0.
 *     7. FNV-1a chain replays byte-identically.
 *
 *   v289.1 (named, not implemented): live crash
 *     recovery harness exercising each cascade tier
 *     under fault injection, a WAL-backed σ-log that
 *     survives SIGKILL mid-write, and an executable
 *     `cos rebuild` that demonstrates resume (not
 *     restart) from the last σ-log entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V289_RUIN_VALUE_H
#define COS_V289_RUIN_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V289_N_REMOVAL      4
#define COS_V289_N_CASCADE      4
#define COS_V289_N_PRESERVE     3
#define COS_V289_N_REBUILD      3

typedef struct {
    char  kernel_removed       [14];
    char  survivor             [20];
    bool  survivor_still_works;
} cos_v289_removal_t;

typedef struct {
    int   tier_id;
    char  name[22];
    float resource_cost;
    bool  standalone_viable;
} cos_v289_cascade_t;

typedef struct {
    char  name[32];
    bool  guaranteed;
} cos_v289_preserve_t;

typedef struct {
    char  step_name[22];
    int   step_order;
    bool  possible;
} cos_v289_rebuild_t;

typedef struct {
    cos_v289_removal_t   removal    [COS_V289_N_REMOVAL];
    cos_v289_cascade_t   cascade    [COS_V289_N_CASCADE];
    cos_v289_preserve_t  preserve   [COS_V289_N_PRESERVE];
    cos_v289_rebuild_t   rebuild    [COS_V289_N_REBUILD];

    int   seed_kernels_required;  /* 5 */

    int   n_removal_rows_ok;
    bool  removal_all_survive_ok;
    bool  removal_distinct_ok;

    int   n_cascade_rows_ok;
    bool  cascade_tier_permutation_ok;
    bool  cascade_all_viable_ok;
    bool  cascade_cost_monotone_ok;

    int   n_preserve_rows_ok;
    bool  preserve_all_guaranteed_ok;

    int   n_rebuild_rows_ok;
    bool  rebuild_order_permutation_ok;
    bool  rebuild_all_possible_ok;

    bool  seed_required_ok;

    float sigma_ruin;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v289_state_t;

void   cos_v289_init(cos_v289_state_t *s, uint64_t seed);
void   cos_v289_run (cos_v289_state_t *s);

size_t cos_v289_to_json(const cos_v289_state_t *s,
                         char *buf, size_t cap);

int    cos_v289_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V289_RUIN_VALUE_H */
