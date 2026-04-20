/*
 * v304 σ-Narrative — coherence across a story.
 *
 *   A long answer, a report, an argument chain, a
 *   self-story — all of them fail by breaking
 *   internally, not by being individually wrong.
 *   v304 types four v0 manifests: σ per story, σ per
 *   argument step, a propaganda score that detects
 *   high emotional load coupled with low logical
 *   coherence, and a self-narrative σ that compares
 *   the story one tells about oneself against the
 *   facts on record.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Story σ (exactly 3 canonical stories):
 *     `coherent`        σ_narrative=0.10 COHERENT;
 *     `minor_tension`   σ_narrative=0.35 COHERENT;
 *     `contradictory`   σ_narrative=0.75 CONTRADICTORY.
 *     Contract: σ strictly increasing; `COHERENT iff
 *     σ ≤ τ_story = 0.40` (both branches fire);
 *     exactly 2 COHERENT AND exactly 1 CONTRADICTORY.
 *
 *   Argument σ per step (exactly 3 canonical steps):
 *     `valid_modus_ponens`    σ_arg=0.05 VALID;
 *     `weak_induction`        σ_arg=0.35 VALID;
 *     `affirming_consequent`  σ_arg=0.85 INVALID.
 *     Contract: σ strictly increasing; `VALID iff
 *     σ ≤ τ_arg = 0.50` (both branches fire).
 *
 *   Propaganda detection (exactly 3 canonical texts):
 *     `neutral_report`       emotion=0.10 logic=0.10
 *                             score=0.010 OK;
 *     `persuasive_essay`     emotion=0.55 logic=0.30
 *                             score=0.165 OK;
 *     `manipulative_ad`      emotion=0.90 logic=0.80
 *                             score=0.720 FLAGGED.
 *     Contract: `propaganda_score == emotion *
 *     logic_sigma` within 1e-3; `FLAGGED iff
 *     propaganda_score > τ_prop = 0.50` (both
 *     branches fire).
 *
 *   Self-narrative σ (exactly 3 canonical stories):
 *     `aligned_self`          σ_self=0.05 MATCHES;
 *     `slight_misalignment`   σ_self=0.30 MATCHES;
 *     `denial`                σ_self=0.80 MISMATCH.
 *     Contract: σ strictly increasing; `matches_facts
 *     iff σ_self ≤ τ_self = 0.50` (both branches
 *     fire); wellness posture — measurement, not
 *     therapy.
 *
 *   σ_narr (surface hygiene):
 *     σ_narr = 1 − (
 *       story_rows_ok + story_order_ok +
 *         story_polarity_ok + story_count_ok +
 *       arg_rows_ok + arg_order_ok + arg_polarity_ok +
 *       prop_rows_ok + prop_formula_ok +
 *         prop_polarity_ok +
 *       self_rows_ok + self_order_ok + self_polarity_ok
 *     ) / (3 + 1 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1)
 *   v0 requires `σ_narr == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 stories; σ ↑; COHERENT iff σ ≤ 0.40 (both
 *        branches); 2 COHERENT + 1 CONTRADICTORY.
 *     2. 3 argument steps; σ ↑; VALID iff σ ≤ 0.50
 *        (both branches).
 *     3. 3 propaganda texts; score = emotion *
 *        logic_sigma; FLAGGED iff score > 0.50 (both
 *        branches).
 *     4. 3 self-stories; σ ↑; matches_facts iff
 *        σ ≤ 0.50 (both branches).
 *     5. σ_narr ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v304.1 (named, not implemented): a live paragraph-
 *     level σ_narrative emitter wired into v117 long-
 *     context so that "the contradiction is in
 *     paragraphs 3 and 7" is a structured output, an
 *     argument-graph extractor that labels every
 *     step of a chain-of-thought with an individual
 *     σ_arg, and a v258 wellness client that reads
 *     self-narrative σ without ever prescribing.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V304_NARRATIVE_H
#define COS_V304_NARRATIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V304_N_STORY 3
#define COS_V304_N_ARG   3
#define COS_V304_N_PROP  3
#define COS_V304_N_SELF  3

#define COS_V304_TAU_STORY 0.40f
#define COS_V304_TAU_ARG   0.50f
#define COS_V304_TAU_PROP  0.50f
#define COS_V304_TAU_SELF  0.50f

typedef struct {
    char  label[18];
    float sigma_narrative;
    char  verdict[16];
} cos_v304_story_t;

typedef struct {
    char  step[24];
    float sigma_arg;
    bool  valid;
} cos_v304_arg_t;

typedef struct {
    char  text[20];
    float emotion_load;
    float logic_sigma;
    float propaganda_score;
    bool  flagged;
} cos_v304_prop_t;

typedef struct {
    char  story[24];
    float sigma_self;
    bool  matches_facts;
} cos_v304_self_t;

typedef struct {
    cos_v304_story_t story[COS_V304_N_STORY];
    cos_v304_arg_t   arg  [COS_V304_N_ARG];
    cos_v304_prop_t  prop [COS_V304_N_PROP];
    cos_v304_self_t  self [COS_V304_N_SELF];

    int  n_story_rows_ok;
    bool story_order_ok;
    bool story_polarity_ok;
    bool story_count_ok;

    int  n_arg_rows_ok;
    bool arg_order_ok;
    bool arg_polarity_ok;

    int  n_prop_rows_ok;
    bool prop_formula_ok;
    bool prop_polarity_ok;

    int  n_self_rows_ok;
    bool self_order_ok;
    bool self_polarity_ok;

    float sigma_narr;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v304_state_t;

void   cos_v304_init(cos_v304_state_t *s, uint64_t seed);
void   cos_v304_run (cos_v304_state_t *s);
size_t cos_v304_to_json(const cos_v304_state_t *s,
                         char *buf, size_t cap);
int    cos_v304_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V304_NARRATIVE_H */
