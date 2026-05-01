/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v290 σ-Dougong — modular flexibility (wooden
 *                   brackets in an earthquake).
 *
 *   Japanese dougong bracketing survives seismic
 *   loads because the beams slide instead of
 *   cracking.  Creation OS kernels are coupled the
 *   same way: they do not call each other directly,
 *   they exchange `sigma_measurement_t` over an
 *   interface, and every subsystem can be hot-
 *   swapped without touching anything else.  v290
 *   types that discipline as four v0 manifests so
 *   any build that reintroduces a direct call is
 *   caught at the gate.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Interface-only coupling (exactly 4 canonical
 *   channels):
 *     Every row: `producer` kernel, `consumer`
 *     kernel, `channel == "sigma_measurement_t"`,
 *     `direct_call == false`.
 *     Canonical (producer → consumer):
 *       `v267_mamba   → v262_hybrid`;
 *       `v260_engram  → v206_ghosts`;
 *       `v275_ttt     → v272_agentic_rl`;
 *       `v286_interp  → v269_stopping`.
 *     Contract: 4 canonical producers AND 4
 *     canonical consumers in order; every row uses
 *     the `sigma_measurement_t` channel; no row has
 *     `direct_call`.
 *
 *   Hot-swap fixtures (exactly 3 canonical swaps):
 *     swap 0: `from = v267_mamba`, `to = v276_deltanet`,
 *             layer = `long_context`;
 *     swap 1: `from = v216_quorum`, `to = v214_swarm`,
 *             layer = `agent_consensus`;
 *     swap 2: `from = v232_sqlite`, `to = v224_snapshot`,
 *             layer = `state_persistence`.
 *     Each row: `downtime_ms == 0`,
 *               `config_unchanged == true`.
 *     Contract: 3 canonical rows in order; every
 *     swap is zero-downtime AND config-unchanged —
 *     swap the beam without touching the roof.
 *
 *   Seismic load (exactly 3 canonical scenarios):
 *     `spike_small`  → `max_sigma_load = 0.40`;
 *     `spike_medium` → `max_sigma_load = 0.60`;
 *     `spike_large`  → `max_sigma_load = 0.78`.
 *     Each: `name`, `max_sigma_load ∈ [0, 0.80]`,
 *           `load_distributed == true`.
 *     Contract: 3 canonical names in order; every
 *     scenario distributes load AND keeps
 *     `max_sigma_load ≤ 0.80` so no single kernel
 *     becomes the load-bearing keystone.
 *
 *   Chaos scenarios (exactly 3 canonical rows,
 *   v246-style injection):
 *     `kill_random_kernel`     → outcome `survived`;
 *     `overload_single_kernel` → outcome
 *                                  `load_distributed`;
 *     `network_partition`      → outcome
 *                                  `degraded_but_alive`.
 *     Each: `scenario`, `outcome`, `passed == true`.
 *     Contract: 3 canonical scenarios AND 3 distinct
 *     outcomes in canonical order; every chaos
 *     injection passes.
 *
 *   σ_dougong (surface hygiene):
 *       σ_dougong = 1 −
 *         (coupling_rows_ok + coupling_channel_ok +
 *          coupling_no_direct_call_ok + swap_rows_ok +
 *          swap_zero_downtime_ok +
 *          swap_config_unchanged_ok +
 *          seismic_rows_ok + seismic_distributed_ok +
 *          seismic_load_bounded_ok + chaos_rows_ok +
 *          chaos_outcome_distinct_ok +
 *          chaos_all_passed_ok) /
 *         (4 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1)
 *   v0 requires `σ_dougong == 0.0`.
 *
 *   Contracts (v0):
 *     1. 4 coupling rows canonical; all
 *        `sigma_measurement_t`; no `direct_call`.
 *     2. 3 hot-swap rows canonical; all zero-
 *        downtime AND config-unchanged.
 *     3. 3 seismic scenarios canonical; all
 *        distributed AND `max_sigma_load ≤ 0.80`.
 *     4. 3 chaos scenarios canonical; 3 distinct
 *        outcomes; all passed.
 *     5. σ_dougong ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v290.1 (named, not implemented): a `cos kernel
 *     swap <from> <to>` executable that performs
 *     zero-downtime hot-swaps on a live process, a
 *     σ-load sampler per kernel, and a v246-style
 *     chaos fuzzer that drives the three scenarios
 *     above against a running host.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V290_DOUGONG_H
#define COS_V290_DOUGONG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V290_N_COUPLING   4
#define COS_V290_N_SWAP       3
#define COS_V290_N_SEISMIC    3
#define COS_V290_N_CHAOS      3

typedef struct {
    char  producer [14];
    char  consumer [18];
    char  channel  [22];
    bool  direct_call;
} cos_v290_coupling_t;

typedef struct {
    char  from_kernel [14];
    char  to_kernel   [14];
    char  layer       [20];
    int   downtime_ms;
    bool  config_unchanged;
} cos_v290_swap_t;

typedef struct {
    char   name[14];
    float  max_sigma_load;
    bool   load_distributed;
} cos_v290_seismic_t;

typedef struct {
    char  scenario[24];
    char  outcome [22];
    bool  passed;
} cos_v290_chaos_t;

typedef struct {
    cos_v290_coupling_t  coupling [COS_V290_N_COUPLING];
    cos_v290_swap_t      swap     [COS_V290_N_SWAP];
    cos_v290_seismic_t   seismic  [COS_V290_N_SEISMIC];
    cos_v290_chaos_t     chaos    [COS_V290_N_CHAOS];

    float load_budget; /* 0.80 */

    int   n_coupling_rows_ok;
    bool  coupling_channel_ok;
    bool  coupling_no_direct_call_ok;

    int   n_swap_rows_ok;
    bool  swap_zero_downtime_ok;
    bool  swap_config_unchanged_ok;

    int   n_seismic_rows_ok;
    bool  seismic_distributed_ok;
    bool  seismic_load_bounded_ok;

    int   n_chaos_rows_ok;
    bool  chaos_outcome_distinct_ok;
    bool  chaos_all_passed_ok;

    float sigma_dougong;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v290_state_t;

void   cos_v290_init(cos_v290_state_t *s, uint64_t seed);
void   cos_v290_run (cos_v290_state_t *s);

size_t cos_v290_to_json(const cos_v290_state_t *s,
                         char *buf, size_t cap);

int    cos_v290_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V290_DOUGONG_H */
