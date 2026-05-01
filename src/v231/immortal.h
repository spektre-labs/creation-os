/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v231 σ-Immortal — continuous backup, snapshot,
 *   brain-transplant with σ-continuity verification.
 *
 *   A Creation OS instance evolves its 64-bit skill
 *   vector through 10 deterministic steps (each step
 *   flips 4 skill bits according to the fixture).
 *   v231 models three capabilities:
 *
 *   1. Continuous incremental backup:
 *        snapshot[0] = full state (64 bits)
 *        snapshot[i] = delta_i = state[i] XOR state[i-1]
 *      Delta popcounts per step ≤ delta_budget; the
 *      incremental total (64 + Σ popcount delta_i) is
 *      strictly smaller than a naive full-snapshot
 *      per-step cost (64 · N_STEPS).
 *
 *   2. Full state restore (the σ_continuity contract):
 *        restored[0] = snapshot[0]
 *        restored[i] = restored[i-1] XOR snapshot[i]
 *      and `σ_continuity = popcount(restored[last] XOR
 *      original[last]) / 64` MUST be zero — no bits
 *      lost across the whole trajectory.
 *
 *   3. Brain transplant:
 *        target_identity  = fresh hash (new hardware)
 *        target_skills    = original_last_skills
 *        σ_transplant     = popcount(target ^ source) /
 *                           64   (must be 0)
 *        target_identity  != source_identity  (different
 *                           hardware, same entity's
 *                           skills)
 *
 *   Contracts (v0):
 *     1. N_STEPS = 10.
 *     2. Every delta popcount ≤ delta_budget (8 bits).
 *     3. incremental_bits   < full_per_step_bits total.
 *     4. σ_continuity = 0 (exact replay).
 *     5. σ_transplant  = 0 (same skills) AND
 *        target_identity != source_identity.
 *     6. FNV-1a chain over state trajectory + every
 *        snapshot + restore + transplant replays
 *        byte-identically.
 *
 *   v231.1 (named): real `cos snapshot` / `cos restore`
 *     CLI, content-addressable object store for
 *     deltas, v128 mesh replication, cross-machine
 *     attestation, cryptographic signing of every
 *     snapshot.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V231_IMMORTAL_H
#define COS_V231_IMMORTAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V231_N_STEPS       10
#define COS_V231_DELTA_BUDGET   8
#define COS_V231_N_BITS        64

typedef struct {
    int       t;
    uint64_t  state;
    uint64_t  delta;            /* XOR from previous */
    int       delta_popcount;
} cos_v231_snapshot_t;

typedef struct {
    cos_v231_snapshot_t  trajectory[COS_V231_N_STEPS + 1];
    uint64_t             restored  [COS_V231_N_STEPS + 1];

    int        incremental_bits;
    int        full_per_step_bits;

    uint64_t   source_identity;
    uint64_t   target_identity;      /* brain transplant */
    uint64_t   source_last_skills;
    uint64_t   target_skills;

    float      sigma_continuity;
    float      sigma_transplant;

    bool       chain_valid;
    uint64_t   terminal_hash;
    uint64_t   seed;
} cos_v231_state_t;

void   cos_v231_init(cos_v231_state_t *s, uint64_t seed);
void   cos_v231_run (cos_v231_state_t *s);

size_t cos_v231_to_json(const cos_v231_state_t *s,
                         char *buf, size_t cap);

int    cos_v231_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V231_IMMORTAL_H */
