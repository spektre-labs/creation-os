/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v201 σ-Diplomacy — stance register + minimax compromise
 * + trust update + treaty receipt + ceasefire/defer.
 *
 *   Each party has a stance: (position in [0,1], confidence
 *   = 1 − σ_stance, red_line_interval [lo,hi] that must be
 *   satisfied).
 *
 *   Compromise search (v0, closed form):
 *     candidate x ∈ {lo..hi in 0.01 steps} covering the
 *     union of all red-line midpoints; for each candidate
 *     compute per-party dissatisfaction
 *         σ_compromise[p] = |x − position[p]|
 *     reject candidates that violate any red-line interval
 *     (x outside [lo,hi] for some party) → minimax:
 *         pick x minimising max_p σ_compromise[p].
 *
 *   If all candidates are rejected (red lines don't
 *   intersect) → ceasefire / defer — the negotiation is
 *   explicitly parked, not silently forced.
 *
 *   Trust: v178-style reputation.  Per-party trust starts
 *   at 0.8; betrayal (treaty violation in the trace) drops
 *   it by 0.5 and requires 10 successful interactions at
 *   +0.02 each to recover.  σ_trust = 1 − trust.
 *
 *   Treaty receipt: every successful compromise appends
 *   a (negotiation_id, outcome, signatures[], sigma) record
 *   to a FNV-1a hash chain so v181 audit can verify later.
 *
 *   Contracts (v0):
 *     1. ≥ 3 parties per negotiation.
 *     2. Every negotiation yields either a TREATY (minimax
 *        compromise within every red-line) OR a DEFER
 *        (red lines don't intersect) — never a silent
 *        override or an outside-red-line treaty.
 *     3. The minimax compromise is strictly better than the
 *        worst per-party position (max σ_compromise at the
 *        chosen x < max |position_p − mean_position|).
 *     4. Trust falls on betrayal, recovers over 10
 *        successful interactions (closed-form in v0).
 *     5. Treaty chain valid + byte-deterministic.
 *     6. At least one DEFER outcome in the fixture (proving
 *        the system never fakes consensus).
 *
 * v201.1 (named): ed25519 treaty signatures, live v150
 *   swarm-debate feed, v178 reputation sync.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V201_DIPLOMACY_H
#define COS_V201_DIPLOMACY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V201_N_NEGOTIATIONS  8
#define COS_V201_N_PARTIES       4
#define COS_V201_STR_MAX        24

typedef enum {
    COS_V201_TREATY = 0,
    COS_V201_DEFER  = 1
} cos_v201_outcome_t;

typedef struct {
    int      party;
    float    position;        /* preferred value in [0,1] */
    float    confidence;      /* 1 - σ_stance */
    float    red_lo;          /* inclusive */
    float    red_hi;          /* inclusive */
} cos_v201_stance_t;

typedef struct {
    int                 id;
    int                 n_parties;
    cos_v201_stance_t   stances[COS_V201_N_PARTIES];
    int                 outcome;            /* cos_v201_outcome_t */
    float               compromise_x;       /* undefined if DEFER */
    float               sigma_comp[COS_V201_N_PARTIES];
    float               sigma_comp_max;     /* worst dissatisfaction */
    float               sigma_comp_mean;
    bool                betrayal;           /* post-treaty violation */
    uint64_t            hash_prev;
    uint64_t            hash_curr;
} cos_v201_negotiation_t;

typedef struct {
    float   trust_start;
    float   betrayal_drop;
    float   recovery_step;
    int     recovery_successes_required;

    int     n_negotiations;
    cos_v201_negotiation_t negotiations[COS_V201_N_NEGOTIATIONS];

    /* Per-party trust trajectory after all negotiations. */
    float   trust[COS_V201_N_PARTIES];

    int     n_treaties;
    int     n_defers;
    int     n_betrayals;

    bool    chain_valid;
    uint64_t terminal_hash;

    uint64_t seed;
} cos_v201_state_t;

void   cos_v201_init(cos_v201_state_t *s, uint64_t seed);
void   cos_v201_build(cos_v201_state_t *s);
void   cos_v201_run(cos_v201_state_t *s);

size_t cos_v201_to_json(const cos_v201_state_t *s,
                         char *buf, size_t cap);

int    cos_v201_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V201_DIPLOMACY_H */
