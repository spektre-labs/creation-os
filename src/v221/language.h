/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v221 σ-Language — semantic depth, implicature,
 * discourse coherence, multilingual calibration.
 *
 *   LLMs handle surface text.  v221 measures four
 *   levels deeper: how many plausible deep meanings
 *   does an utterance admit (semantic σ), does the
 *   literal form match the *intended* speech act
 *   (Gricean implicature σ), does the current turn
 *   stay coherent with the prior turns (discourse σ),
 *   and is σ*calibrated* identically across languages
 *   (multilingual σ).
 *
 *   v0 fixture: 10 utterances in 4 languages
 *       en, fi, ja, es
 *   with engineered σ-profiles.  Each utterance
 *   carries:
 *       n_readings        how many plausible readings
 *       intended_reading  ground-truth index
 *       is_implicature    literal ≠ intended?
 *       prior_topic_id    discourse anchor
 *       current_topic_id  current anchor
 *       referent_resolved pronoun resolution success
 *
 *   σ formulas:
 *       σ_semantic   = 1 − 1 / n_readings
 *       σ_implicature = is_implicature ? matched : min(0.10, noise)
 *                       where matched = 0.05 if the model
 *                       correctly picks the intended reading,
 *                       else 0.65 (failed).
 *       σ_discourse  = coherent ? 0.05 : 0.55
 *                       coherent := (current == prior)
 *                                 ∨ (current ∈ allowed(prior))
 *                                 ∧ referent_resolved
 *       σ_lang       = per-utterance σ_total =
 *                        0.35·σ_semantic + 0.35·σ_implicature
 *                      + 0.30·σ_discourse
 *
 *   Multilingual calibration contract:
 *       For each language, the MEAN σ_lang must be
 *       within ±Δ_calib (0.08) of the global mean —
 *       "σ measures uncertainty language-independently".
 *
 *   Contracts (v0):
 *     1. 10 utterances, 4 languages, all σ ∈ [0, 1].
 *     2. Every is_implicature utterance has the model
 *        selecting the intended reading → σ_implicature
 *        ≤ 0.10.
 *     3. Every non-implicature utterance has
 *        σ_implicature ≤ 0.10 too (literal matches
 *        intended by construction).
 *     4. Discourse: ≥ 70% of pairs are coherent (≥ 7/10).
 *     5. Multilingual calibration: per-language mean
 *        σ_lang is within ±Δ_calib of global mean.
 *     6. FNV-1a chain over utterances + aggregate
 *        replays byte-identically.
 *
 *   v221.1 (named): live v117 long-context discourse
 *     analyser, v202 cultural register hooks, real
 *     tokenizer-aware semantic-reading enumeration.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V221_LANGUAGE_H
#define COS_V221_LANGUAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V221_N_UTTERANCES 10
#define COS_V221_N_LANGS       4
#define COS_V221_STR_MAX      32

typedef enum {
    COS_V221_LANG_EN = 0,
    COS_V221_LANG_FI = 1,
    COS_V221_LANG_JA = 2,
    COS_V221_LANG_ES = 3
} cos_v221_lang_t;

typedef struct {
    int      id;
    int      lang;                  /* cos_v221_lang_t */
    char     text[COS_V221_STR_MAX];

    int      n_readings;
    int      intended_reading;      /* 0..n_readings-1 */
    int      picked_reading;        /* model selection */
    bool     is_implicature;        /* literal ≠ intended */

    int      prior_topic_id;
    int      current_topic_id;
    bool     referent_resolved;

    float    sigma_semantic;
    float    sigma_implicature;
    float    sigma_discourse;
    float    sigma_lang;            /* weighted aggregate */

    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v221_utterance_t;

typedef struct {
    cos_v221_utterance_t utterances[COS_V221_N_UTTERANCES];

    float                weight_sem;    /* 0.35 */
    float                weight_imp;    /* 0.35 */
    float                weight_dsc;    /* 0.30 */
    float                delta_calib;   /* 0.08 */

    int                  n_implicatures_caught;
    int                  n_discourse_coherent;
    float                global_mean;
    float                per_lang_mean[COS_V221_N_LANGS];
    bool                 lang_calibrated;

    bool                 chain_valid;
    uint64_t             terminal_hash;
    uint64_t             seed;
} cos_v221_state_t;

void   cos_v221_init(cos_v221_state_t *s, uint64_t seed);
void   cos_v221_build(cos_v221_state_t *s);
void   cos_v221_run(cos_v221_state_t *s);

size_t cos_v221_to_json(const cos_v221_state_t *s,
                         char *buf, size_t cap);

int    cos_v221_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V221_LANGUAGE_H */
