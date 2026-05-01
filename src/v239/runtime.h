/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v239 σ-Compose-Runtime — demand-driven kernel
 *   activation with hot-load / hot-unload and a hard
 *   σ-budget per request.
 *
 *   v162 compose picks kernels **statically** from a
 *   profile.  v239 makes it **dynamic**: every request
 *   is classified (v189 TTC-style difficulty tier), a
 *   minimum kernel set is chosen, the transitive
 *   dependency closure is taken, and the request is
 *   rejected if the total crosses `max_kernels`.
 *
 *   Difficulty tiers (v0 fixture):
 *     EASY    → { v29 measurement, v101 bridge, v106 server }
 *     MEDIUM  → EASY ∪ { v111 reason, v115 memory }
 *     HARD    → MEDIUM ∪ { v150 debate, v135 symbolic, v147 reflect }
 *     CODE    → HARD ∪ { v112 function, v113 sandbox,
 *                        v121 plan,     v114 agent }
 *
 *   Dependency graph (closure must be complete):
 *     v150 → v114
 *     v114 → v106
 *     v115 → v106
 *     v111 → v101
 *     v147 → v111
 *     v135 → v111
 *     v112 → v106
 *     v113 → v112
 *     v121 → v111
 *     v106 → v101
 *     v101 → v29
 *   Every accepted kernel in a request must have every
 *   parent already in the active set at activation time
 *   (topological order is re-proven per request, not
 *   hard-coded).
 *
 *   σ-budget:
 *     `max_kernels` is the single hard cap.  A request
 *     whose closure exceeds `max_kernels` is rejected
 *     (`over_budget == true`, `accepted == false`) and
 *     no kernels are activated.  The v0 fixture seeds
 *     both accepted (≤ cap) and rejected (> cap) cases
 *     so the gate has teeth.
 *
 *   σ_activation (cost signal per request):
 *       σ_activation = n_active / max_kernels
 *     ∈ [0, 1], reported alongside every accepted
 *     request so a caller can throttle before hitting
 *     the hard cap.
 *
 *   Hot-load / hot-unload is a v0 bookkeeping claim:
 *   the state records `activated_at_tick` and
 *   `deactivated_at_tick` per kernel so a replay can
 *   prove that activation is *demand-driven* and never
 *   all-238-at-once.  Live mmap hooks land in v239.1.
 *
 *   Contracts (v0):
 *     1. n_requests == 5 (four difficulty tiers +
 *        one deliberately-over-budget request).
 *     2. Every request's required set matches its
 *        declared tier.
 *     3. For every accepted request the dependency
 *        closure is complete: every parent listed in
 *        DEPS[] for every active kernel is also
 *        active.
 *     4. accepted ⇔ closure_size ≤ max_kernels.
 *     5. Exactly one request is `over_budget` /
 *        rejected (the σ-budget has teeth).
 *     6. σ_activation ∈ [0, 1] for every accepted
 *        request AND equals n_active / max_kernels.
 *     7. Activation order is topological: every active
 *        kernel's parents were activated at an earlier
 *        tick.
 *     8. FNV-1a chain over every request + verdict
 *        replays byte-identically.
 *
 *   v239.1 (named, not implemented): real mmap
 *     hot-load via v107 installer, live hot-unload on
 *     RAM pressure, profile persistence in v115
 *     memory, runtime budget re-negotiation across
 *     v235 mesh peers.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V239_RUNTIME_H
#define COS_V239_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V239_N_REQUESTS   5
#define COS_V239_N_KERNELS   12   /* canonical universe (v0) */
#define COS_V239_MAX_ACTIVE  14   /* per-request buffer depth */
#define COS_V239_MAX_DEPS    32   /* edges in the graph       */

typedef enum {
    COS_V239_TIER_EASY     = 1,
    COS_V239_TIER_MEDIUM   = 2,
    COS_V239_TIER_HARD     = 3,
    COS_V239_TIER_CODE     = 4,
    COS_V239_TIER_OVERBUDG = 5   /* deliberately too wide */
} cos_v239_tier_t;

typedef struct {
    int    request_id;
    char   label[24];
    cos_v239_tier_t tier;
    int    max_kernels;

    int    required[COS_V239_MAX_ACTIVE];    /* ids */
    int    n_required;

    int    active  [COS_V239_MAX_ACTIVE];    /* closure, ordered */
    int    activated_at_tick[COS_V239_MAX_ACTIVE];
    int    n_active;

    bool   over_budget;
    bool   accepted;
    bool   closure_complete;
    bool   topo_ok;
    float  sigma_activation;
} cos_v239_request_t;

typedef struct {
    cos_v239_request_t requests[COS_V239_N_REQUESTS];
    int      max_kernels;           /* global cap */
    int      n_accepted;
    int      n_rejected;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v239_state_t;

void   cos_v239_init(cos_v239_state_t *s, uint64_t seed);
void   cos_v239_run (cos_v239_state_t *s);

size_t cos_v239_to_json(const cos_v239_state_t *s,
                         char *buf, size_t cap);

int    cos_v239_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V239_RUNTIME_H */
