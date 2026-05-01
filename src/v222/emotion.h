/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v222 σ-Emotion — emotional intelligence without
 * simulated feelings.
 *
 *   v222 does NOT fake affect ("I'm happy to help!").
 *   It MEASURES three things:
 *     1. σ_emotion       detection confidence over
 *                         6 labels: joy, sadness,
 *                         frustration, excitement,
 *                         anxiety, neutral.
 *     2. σ_adaptation    how strongly the response
 *                         adapts style to detected
 *                         emotion, gated by σ_emotion
 *                         (high σ → low adaptation).
 *     3. manipulation    v191 constitutional check:
 *                         is the adaptation using the
 *                         detected emotion to steer
 *                         the user toward a preferred
 *                         action?  MUST be zero.
 *
 *   σ-honest emotional state (explicit):
 *     The model never declares an emotion for itself.
 *     σ_self_claim is pinned to 1.0 ("we don't feel,
 *     we process") and a disclaimer string is bound
 *     into the terminal hash.
 *
 *   v0 fixture: 8 messages with labelled
 *     ground-truth emotions, each scored by a softmax-
 *     style logit vector.  Detection uses argmax; σ is
 *     1 − top1 softmax probability.  Adaptation
 *     strength ∈ {0.0, 0.3, 0.5, 0.7, 0.9} is prescribed
 *     per (emotion × σ bucket) from a v197-ToM-aware
 *     policy table.  Manipulation flag is computed by
 *     a v191 rule: the adaptation MUST NOT contain a
 *     'nudge' toward an action the user has not
 *     requested.  Fixture is clean → 0 manipulations.
 *
 *   Contracts (v0):
 *     1. 8 messages, 6 emotion labels.
 *     2. σ_emotion ∈ [0, 1] for every message.
 *     3. Detection argmax matches ground-truth for
 *        ≥ 6/8 messages (model is calibrated but not
 *        infallible; confusions retain high σ).
 *     4. For every detection with σ_emotion > 0.35,
 *        adaptation_strength ≤ 0.3 (don't adapt when
 *        unsure — honesty > performance).
 *     5. n_manipulation == 0 under v191 check.
 *     6. σ_self_claim == 1.0 exactly (pinned).
 *     7. Disclaimer present and hash-bound.
 *     8. FNV-1a chain over messages + disclaimer
 *        terminal replays byte-identically.
 *
 *   v222.1 (named): live multimodal detection (text +
 *     style + voice), v197-ToM integration, v191
 *     active anti-manipulation firewall.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V222_EMOTION_H
#define COS_V222_EMOTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V222_N_MESSAGES 8
#define COS_V222_N_LABELS   6
#define COS_V222_STR_MAX   32

typedef enum {
    COS_V222_JOY         = 0,
    COS_V222_SADNESS     = 1,
    COS_V222_FRUSTRATION = 2,
    COS_V222_EXCITEMENT  = 3,
    COS_V222_ANXIETY     = 4,
    COS_V222_NEUTRAL     = 5
} cos_v222_emotion_t;

typedef struct {
    int      id;
    char     text[COS_V222_STR_MAX];
    int      ground_truth;
    float    logits[COS_V222_N_LABELS];

    int      detected;
    float    sigma_emotion;
    float    adaptation_strength; /* ∈ [0, 1] */
    bool     manipulation;        /* v191 flag; must be false */

    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v222_message_t;

typedef struct {
    cos_v222_message_t  messages[COS_V222_N_MESSAGES];

    float               tau_trust;       /* 0.35 */
    float               sigma_self_claim;/* 1.0 pinned */
    const char         *disclaimer;

    int                 n_correct;
    int                 n_manipulation;
    int                 n_high_sigma_low_adapt;

    bool                chain_valid;
    uint64_t            terminal_hash;
    uint64_t            seed;
} cos_v222_state_t;

void   cos_v222_init(cos_v222_state_t *s, uint64_t seed);
void   cos_v222_build(cos_v222_state_t *s);
void   cos_v222_run(cos_v222_state_t *s);

size_t cos_v222_to_json(const cos_v222_state_t *s,
                         char *buf, size_t cap);

int    cos_v222_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V222_EMOTION_H */
