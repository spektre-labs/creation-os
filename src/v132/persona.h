/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v132 σ-Persona — adaptive user profile.
 *
 * Each user is different.  v132 tracks per-topic σ to infer
 * expertise level, ingests correction feedback to update style
 * preferences (length, tone), and persists the whole profile to a
 * human-readable TOML file at ~/.creation-os/persona.toml.
 *
 * Three σ-governed ideas:
 *
 *   1. Expertise from σ.  Running EMA of σ_product per topic:
 *      low mean σ → user is confident on this topic (advanced).
 *      High mean σ → user is learning (beginner).  Four-level
 *      staircase: beginner (σ ≥ 0.60), intermediate (0.35–0.60),
 *      advanced (0.15–0.35), expert (σ < 0.15).
 *
 *   2. Correction feedback.  When the user says "too long",
 *      style.length = clamp(length − 1).  "Too technical" nudges
 *      the tone toward simple.  Feedback is *state*, not a fine-
 *      tune — the LoRA adapter side of the loop is v124/v125.
 *
 *   3. TOML persistence (mutable by hand).  Humans can read and
 *      override the profile at any time.  The writer emits a
 *      canonical, idempotent rendering so diffs stay clean.
 *
 * Multi-user: v132 is *per-node* state.  It never federates through
 * v129 — personas stay local by construction (GDPR-friendly).
 */
#ifndef COS_V132_PERSONA_H
#define COS_V132_PERSONA_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V132_MAX_TOPICS        32
#define COS_V132_TOPIC_NAME        64
#define COS_V132_LANG_LEN           8
#define COS_V132_EMA_ALPHA_DEFAULT 0.30f

typedef enum {
    COS_V132_LEVEL_UNKNOWN      = 0,
    COS_V132_LEVEL_BEGINNER     = 1,
    COS_V132_LEVEL_INTERMEDIATE = 2,
    COS_V132_LEVEL_ADVANCED     = 3,
    COS_V132_LEVEL_EXPERT       = 4,
} cos_v132_level_t;

typedef enum {
    COS_V132_LENGTH_TERSE   = 0,
    COS_V132_LENGTH_CONCISE = 1,
    COS_V132_LENGTH_NORMAL  = 2,
    COS_V132_LENGTH_VERBOSE = 3,
} cos_v132_length_t;

typedef enum {
    COS_V132_TONE_DIRECT = 0,
    COS_V132_TONE_NEUTRAL = 1,
    COS_V132_TONE_FORMAL = 2,
} cos_v132_tone_t;

typedef enum {
    COS_V132_STYLE_SIMPLE     = 0,
    COS_V132_STYLE_NEUTRAL    = 1,
    COS_V132_STYLE_TECHNICAL  = 2,
} cos_v132_style_t;

typedef enum {
    COS_V132_FEEDBACK_TOO_LONG       = 1,
    COS_V132_FEEDBACK_TOO_SHORT      = 2,
    COS_V132_FEEDBACK_TOO_TECHNICAL  = 3,
    COS_V132_FEEDBACK_TOO_SIMPLE     = 4,
    COS_V132_FEEDBACK_TOO_FORMAL     = 5,
    COS_V132_FEEDBACK_TOO_DIRECT     = 6,
} cos_v132_feedback_t;

typedef struct cos_v132_topic_expertise {
    char             topic[COS_V132_TOPIC_NAME];
    float            ema_sigma;
    int              samples;
    cos_v132_level_t level;
} cos_v132_topic_expertise_t;

typedef struct cos_v132_persona {
    cos_v132_topic_expertise_t topics[COS_V132_MAX_TOPICS];
    int                 n_topics;
    cos_v132_length_t   style_length;
    cos_v132_tone_t     style_tone;
    cos_v132_style_t    style_detail;
    char                language[COS_V132_LANG_LEN];
    float               ema_alpha;        /* EMA mixing weight */
    int                 adaptation_count; /* interactions processed */
} cos_v132_persona_t;

/* Human-readable labels for serialisation / CLI. */
const char *cos_v132_level_label   (cos_v132_level_t);
const char *cos_v132_length_label  (cos_v132_length_t);
const char *cos_v132_tone_label    (cos_v132_tone_t);
const char *cos_v132_detail_label  (cos_v132_style_t);

/* Level staircase from an EMA-σ value. */
cos_v132_level_t cos_v132_level_from_sigma(float ema_sigma);

/* Initialise defaults (normal length, neutral tone, English). */
void cos_v132_persona_init(cos_v132_persona_t *p);

/* Observe an interaction: updates topic EMA, recomputes level,
 * and bumps adaptation counter.  Returns the new EMA σ for the
 * topic. */
float cos_v132_observe(cos_v132_persona_t *p, const char *topic,
                       float sigma_product);

/* Apply user correction feedback: nudges style state by one step
 * in the requested direction. */
int   cos_v132_apply_feedback(cos_v132_persona_t *p,
                              cos_v132_feedback_t fb);

/* TOML I/O — deterministic layout, self-contained. */
int   cos_v132_write_toml(const cos_v132_persona_t *p, FILE *fp);
int   cos_v132_read_toml (cos_v132_persona_t *p, FILE *fp);

/* JSON summary (for HTTP / health endpoints). */
int   cos_v132_to_json(const cos_v132_persona_t *p,
                       char *out, size_t cap);

int   cos_v132_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
