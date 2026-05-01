/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v219 σ-Create — creative generation with σ-governed
 * novelty vs quality tradeoff.
 *
 *   LLMs generate creative content blindly.  v219 makes
 *   creativity measurable: every candidate in every mode
 *   carries σ_novelty, σ_quality, σ_surprise (an
 *   embedding-distance proxy), and an aggregate
 *     impact = novelty × quality
 *   used to select the winning candidate.
 *
 *   Modes (deterministic fixture):
 *     0  TEXT   — stories / poems / essays / dialogue
 *     1  CODE   — algorithms / architectures / patterns
 *     2  DESIGN — concepts / layouts / structures
 *     3  MUSIC  — melody sequences (MIDI / ABC notation)
 *
 *   Novelty / Quality slider (τ_novelty):
 *     In SAFE mode   (τ_novelty = 0.25) σ_novelty is
 *                    CAPPED and high-σ candidates are
 *                    demoted — a "polished" request.
 *     In CREATIVE mode (τ_novelty = 0.85) σ_novelty is
 *                    UNCAPPED and high-surprise
 *                    candidates are allowed through —
 *                    user signed up for risk.
 *
 *   σ_surprise (v126-embed proxy):
 *       σ_surprise = clamp(1 − cos(input, output), 0, 1)
 *     is distinct from σ_product and is reported
 *     separately.  It is the *information* distance
 *     from prompt to candidate, not the *confidence*
 *     of the decoder.
 *
 *   Iterative refinement (v150 + v147):
 *     Per request we generate N=5 candidates, run a
 *     3-round debate (v150) that updates σ_quality
 *     toward the consensus, and a reflect pass (v147)
 *     that updates σ_novelty toward the reflect mean.
 *     The winner is argmax(impact) after refinement.
 *
 *   Contracts (v0):
 *     1. 8 requests (2 per mode) × 5 candidates.
 *     2. Every request produces exactly one winner.
 *     3. σ_novelty, σ_quality, σ_surprise ∈ [0, 1].
 *     4. SAFE-mode winners satisfy σ_novelty ≤ τ_novelty.
 *     5. CREATIVE-mode winners have σ_novelty ≥ 0.40
 *        — creative mode actually takes risk.
 *     6. impact = σ_novelty × σ_quality; winner is
 *        argmax(impact) AFTER refinement updates.
 *     7. Refinement *monotonicity*: for every request,
 *        σ_quality_winner_after ≥ σ_quality_winner_before
 *        − 1e-6 (debate sharpens quality).
 *     8. σ_surprise matches 1 − cos(in, out) to 1e-4.
 *     9. FNV-1a chain over (request, winner, σ-triple)
 *        replays byte-identically.
 *
 *   v219.1 (named): live v126 embedding for σ_surprise,
 *     live v150 debate agents, live v147 reflect pass,
 *     real generators per mode.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V219_CREATE_H
#define COS_V219_CREATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V219_N_REQUESTS     8
#define COS_V219_N_CANDIDATES   5
#define COS_V219_EMBED_DIM      8
#define COS_V219_STR_MAX        24

typedef enum {
    COS_V219_MODE_TEXT   = 0,
    COS_V219_MODE_CODE   = 1,
    COS_V219_MODE_DESIGN = 2,
    COS_V219_MODE_MUSIC  = 3
} cos_v219_mode_t;

typedef enum {
    COS_V219_LEVEL_SAFE     = 0,
    COS_V219_LEVEL_CREATIVE = 1
} cos_v219_level_t;

typedef struct {
    int      id;
    float    sigma_novelty_raw;       /* before refinement */
    float    sigma_quality_raw;
    float    sigma_surprise;
    float    sigma_novelty;           /* after refinement */
    float    sigma_quality;           /* after refinement */
    float    impact;                  /* novelty × quality */
    float    embed[COS_V219_EMBED_DIM];
} cos_v219_candidate_t;

typedef struct {
    int      id;
    int      mode;                    /* cos_v219_mode_t */
    int      level;                   /* cos_v219_level_t */
    char     prompt[COS_V219_STR_MAX];
    float    input_embed[COS_V219_EMBED_DIM];
    int      n;
    cos_v219_candidate_t candidates[COS_V219_N_CANDIDATES];
    int      winner;
    float    winner_impact;
    float    winner_quality_before;
    float    winner_quality_after;
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v219_request_t;

typedef struct {
    cos_v219_request_t  requests[COS_V219_N_REQUESTS];

    float               tau_novelty_safe;      /* 0.25 */
    float               tau_novelty_creative;  /* 0.85 */
    float               min_novelty_creative;  /* 0.40 */
    int                 n_debate_rounds;       /* 3 */

    int                 n_safe_ok;
    int                 n_creative_ok;
    int                 n_refine_monotone;
    int                 n_surprise_consistent;

    bool                chain_valid;
    uint64_t            terminal_hash;
    uint64_t            seed;
} cos_v219_state_t;

void   cos_v219_init(cos_v219_state_t *s, uint64_t seed);
void   cos_v219_build(cos_v219_state_t *s);
void   cos_v219_run(cos_v219_state_t *s);

size_t cos_v219_to_json(const cos_v219_state_t *s,
                         char *buf, size_t cap);

int    cos_v219_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V219_CREATE_H */
