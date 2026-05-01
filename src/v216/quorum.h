/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v216 σ-Quorum — gradual consensus with minority voice.
 *
 *   v178 Byzantine consensus is binary (accept / reject).
 *   v216 makes consensus *gradual*: agents vote with their
 *   σ-profiles, and the outcome scales with collective
 *   certainty rather than vote count.
 *
 *       σ_collective = min(1, mean σ_agents_for_majority
 *                          + minority_penalty)
 *
 *   where minority_penalty raises σ_collective whenever a
 *   minority faction holds *low-σ* evidence against the
 *   majority — their certainty counts more than their
 *   count.
 *
 *   Action scaling:
 *     σ_collective < τ_quorum   → BOLD     (commit)
 *     σ_collective < τ_deadlock → CAUTIOUS (partial commit)
 *     else if rounds < R_max    → DEBATE   (re-run)
 *     else                        → DEFER    (log + route to v201)
 *
 *   Minority voice preservation:
 *     Every agent with sigma_agent < s_minority (0.20) whose
 *     vote disagrees with the majority is captured in the
 *     audit record as `minority_voice_captured = true`.
 *     "Consensus is X but agent C is confident in Y."
 *
 *   v0 fixture: 5 proposals × 7 agents.  Engineered
 *     decisions:
 *       P0 unanimous confident yes   → BOLD,    no minority
 *       P1 majority yes but minority confident no → CAUTIOUS,
 *          minority preserved
 *       P2 close split, all uncertain → DEBATE then DEFER to v201
 *       P3 strong yes with one low-σ dissent → CAUTIOUS
 *       P4 everyone uncertain (no quorum possible) → DEFER
 *
 *   Contracts (v0):
 *     1. 5 proposals; σ_collective ∈ [0, 1] everywhere.
 *     2. At least one BOLD (σ_collective < τ_quorum 0.30).
 *     3. At least one CAUTIOUS (τ_quorum ≤ σ_collective
 *        < τ_deadlock 0.55).
 *     4. At least one DEFER (σ_collective ≥ τ_deadlock
 *        after R_max rounds) — deadlocks are explicitly
 *        handled, not forced through.
 *     5. At least one proposal has minority_voice_captured=true
 *        — minority is documented.
 *     6. FNV-1a hash chain over proposals replays
 *        byte-identically.
 *
 *   v216.1 (named): live v178 quorum, live v201 diplomacy
 *     compromise-search hook, live v181 streaming audit.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V216_QUORUM_H
#define COS_V216_QUORUM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V216_N_PROPOSALS  5
#define COS_V216_N_AGENTS     7
#define COS_V216_MAX_ROUNDS   3

typedef enum {
    COS_V216_VOTE_NO  = 0,
    COS_V216_VOTE_YES = 1
} cos_v216_vote_t;

typedef enum {
    COS_V216_DEC_BOLD     = 0,
    COS_V216_DEC_CAUTIOUS = 1,
    COS_V216_DEC_DEBATE   = 2,
    COS_V216_DEC_DEFER    = 3
} cos_v216_decision_t;

typedef struct {
    int      id;
    int      votes[COS_V216_N_AGENTS];    /* cos_v216_vote_t */
    float    sigma[COS_V216_N_AGENTS];
    int      n_yes;
    int      n_no;
    int      majority;                    /* COS_V216_VOTE_YES/NO */
    float    sigma_majority_mean;
    float    minority_penalty;
    float    sigma_collective;
    int      rounds_used;
    int      decision;                    /* cos_v216_decision_t */
    bool     minority_voice_captured;
    int      minority_author;              /* -1 if none */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v216_proposal_t;

typedef struct {
    int                     n;
    cos_v216_proposal_t     proposals[COS_V216_N_PROPOSALS];

    float                   tau_quorum;     /* 0.30 */
    float                   tau_deadlock;   /* 0.55 */
    float                   s_minority;     /* 0.20 */
    int                     r_max;          /* 3 */

    int                     n_bold;
    int                     n_cautious;
    int                     n_defer;
    int                     n_minority_kept;

    bool                    chain_valid;
    uint64_t                terminal_hash;
    uint64_t                seed;
} cos_v216_state_t;

void   cos_v216_init(cos_v216_state_t *s, uint64_t seed);
void   cos_v216_build(cos_v216_state_t *s);
void   cos_v216_run(cos_v216_state_t *s);

size_t cos_v216_to_json(const cos_v216_state_t *s,
                         char *buf, size_t cap);

int    cos_v216_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V216_QUORUM_H */
