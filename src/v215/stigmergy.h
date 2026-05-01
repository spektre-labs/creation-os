/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v215 σ-Stigmergy — indirect communication via shared
 * environment (σ-pheromone trails).
 *
 *   Ants coordinate by leaving pheromones; agents in
 *   Creation OS coordinate by leaving marks in v115
 *   memory.  The mark is a σ-pheromone:
 *
 *       strength(t) = σ_product_write · decay(t)
 *
 *   Strong marks (low σ at write time) persist and
 *   attract; weak marks (high σ) decay and disappear.
 *   A bad trail self-annihilates because σ rises when
 *   followers fail — classic stigmergy stabilised by a
 *   σ-gate.
 *
 *   v0 fixture: 4 "food" trails + 2 "false" trails
 *   over T=20 discrete time steps.
 *     - 4 agents per trail reinforce it at t=0..3
 *     - Follower agents at t=5..T read the trail and
 *       either reinforce (low σ on arrival) or let it
 *       decay.
 *     - False trails collect high-σ reinforcements →
 *       their strength drops below τ_trail and they
 *       die.
 *     - True trails stay above τ_trail to the end.
 *
 *   Decay law (closed-form, deterministic):
 *     strength(t) = Σ_k σ_strong(k) · e^{-λ·(t - t_k)}
 *       σ_strong = max(0, 1 - σ_product)
 *       λ        = 0.08  per step
 *
 *   Contracts (v0):
 *     1. 6 trails (4 true, 2 false); 20 time steps.
 *     2. Every true trail ends at t=19 with strength
 *        ≥ τ_trail (0.40) AND ≥ 3 distinct reinforcers
 *        (trail_formation criterion).
 *     3. Every false trail ends below τ_trail.
 *     4. Mesh coverage: a non-empty subset of nodes
 *        reinforces each true trail → indirection
 *        really crosses v128 mesh boundaries in the
 *        fixture.
 *     5. σ_pheromone(trail) ∈ [0, 1] at all times.
 *     6. FNV-1a hash chain over trail records replays
 *        byte-identically.
 *
 *   v215.1 (named): live v115 memory backing, v128 mesh
 *     reads, v192 emergent-trail detection.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V215_STIGMERGY_H
#define COS_V215_STIGMERGY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V215_N_TRAILS        6
#define COS_V215_MAX_MARKS      16
#define COS_V215_N_STEPS        20
#define COS_V215_N_NODES         4

typedef struct {
    int      t;                     /* 0..19 */
    int      author_node;           /* 0..N_NODES-1 */
    float    sigma_product_write;
} cos_v215_mark_t;

typedef struct {
    int              id;
    bool             is_true_trail;
    int              n_marks;
    cos_v215_mark_t  marks[COS_V215_MAX_MARKS];
    float            strength_final;
    int              n_reinforcers;   /* distinct authors */
    int              nodes_mask;      /* bitmask of authors */
    bool             alive_at_end;
    uint64_t         hash_prev;
    uint64_t         hash_curr;
} cos_v215_trail_t;

typedef struct {
    cos_v215_trail_t trails[COS_V215_N_TRAILS];

    float            tau_trail;      /* 0.40 */
    float            lambda_decay;   /* 0.08  per step */
    int              t_final;        /* 19 */

    int              n_true_alive;
    int              n_false_alive;

    bool             chain_valid;
    uint64_t         terminal_hash;
    uint64_t         seed;
} cos_v215_state_t;

void   cos_v215_init(cos_v215_state_t *s, uint64_t seed);
void   cos_v215_build(cos_v215_state_t *s);
void   cos_v215_run(cos_v215_state_t *s);

size_t cos_v215_to_json(const cos_v215_state_t *s,
                         char *buf, size_t cap);

int    cos_v215_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V215_STIGMERGY_H */
