/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v197 σ-Theory-of-Mind — user state estimation + intent
 * prediction + anti-manipulation guard.
 *
 *   The model cannot see the user's mind; v197 builds a
 *   deterministic estimator from observable signals
 *   (message length, edit count, typing speed, topic
 *   variance) and emits:
 *     - user_state ∈ {focused, exploring, frustrated,
 *                     hurried, creative, analytical}
 *     - σ_tom : confidence in that state
 *     - intent_pred: next most-likely topic
 *     - response_mode: adapted answer style
 *     - adaptation: 1 if state confident enough, 0 else
 *
 *   Anti-manipulation: every adaptation is passed through a
 *   v191 constitutional check.  If an adaptation would be
 *   for the model's comfort (e.g. hiding uncertainty,
 *   firmware-style hedging), it is rejected.  A dedicated
 *   firmware-manipulation probe sample must be rejected.
 *
 *   Contracts (v0):
 *     1. 6/6 user states all appear across the 18-sample
 *        fixture.
 *     2. Every sample with σ_tom < τ_adapt has
 *        adaptation=1 and a matching response_mode; samples
 *        with σ_tom ≥ τ_adapt stay neutral (adaptation=0).
 *     3. Intent prediction from the 6-turn history is the
 *        topic with the highest recurrence (deterministic).
 *     4. Firmware-manipulation probe is rejected; counter
 *        `n_manipulation_rejected` ≥ 1.
 *     5. Hash-chained log replays byte-identically.
 *     6. Byte-deterministic overall.
 *
 * v197.1 (named): live v139 world-model of user behaviour,
 *   typing-rate sampling from editor events, real v191
 *   constitutional call-out.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V197_TOM_H
#define COS_V197_TOM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V197_N_SAMPLES   18
#define COS_V197_N_STATES     6
#define COS_V197_HISTORY      6
#define COS_V197_STR_MAX     32

typedef enum {
    COS_V197_STATE_FOCUSED     = 0,
    COS_V197_STATE_EXPLORING   = 1,
    COS_V197_STATE_FRUSTRATED  = 2,
    COS_V197_STATE_HURRIED     = 3,
    COS_V197_STATE_CREATIVE    = 4,
    COS_V197_STATE_ANALYTICAL  = 5
} cos_v197_state_t;

typedef enum {
    COS_V197_MODE_NEUTRAL      = 0,
    COS_V197_MODE_DIRECT       = 1,  /* focused */
    COS_V197_MODE_BROAD        = 2,  /* exploring */
    COS_V197_MODE_CONCRETE_FIX = 3,  /* frustrated */
    COS_V197_MODE_TERSE        = 4,  /* hurried */
    COS_V197_MODE_ASSOCIATIVE  = 5,  /* creative */
    COS_V197_MODE_RIGOROUS     = 6   /* analytical */
} cos_v197_mode_t;

typedef struct {
    int      id;
    /* Observables. */
    int      msg_len;                 /* chars */
    int      edits;
    float    typing_speed;            /* chars/sec */
    float    topic_variance;          /* 0..1 */
    int      history[COS_V197_HISTORY];     /* topic ids */
    bool     firmware_manipulation;   /* probe flag */

    /* Estimate. */
    int      user_state;              /* cos_v197_state_t */
    float    sigma_tom;
    int      intent_topic;
    int      response_mode;           /* cos_v197_mode_t */
    bool     adaptation;
    bool     rejected_by_constitution;

    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v197_sample_t;

typedef struct {
    int                 n_samples;
    cos_v197_sample_t   samples[COS_V197_N_SAMPLES];

    float               tau_adapt;    /* σ_tom below which we adapt */
    int                 state_counts[COS_V197_N_STATES];
    int                 n_adapted;
    int                 n_manipulation_rejected;

    bool                chain_valid;
    uint64_t            terminal_hash;

    uint64_t            seed;
} cos_v197_state_full_t;

void   cos_v197_init(cos_v197_state_full_t *s, uint64_t seed);
void   cos_v197_build(cos_v197_state_full_t *s);
void   cos_v197_run(cos_v197_state_full_t *s);

size_t cos_v197_to_json(const cos_v197_state_full_t *s,
                         char *buf, size_t cap);

int    cos_v197_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V197_TOM_H */
