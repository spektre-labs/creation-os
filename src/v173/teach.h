/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v173 σ-Teach — Socratic mode, curriculum generation,
 * adaptive exercises, progress tracking, σ-honest teaching.
 *
 * A model that can *teach*, not only answer:
 *
 *   cos teach "explain σ-theory"
 *
 * does not dump a monologue.  It:
 *
 *   1. Socratic probe — a few diagnostic questions.  Student
 *      answers with correctness p ∈ {0.0 .. 1.0}; the kernel
 *      estimates a per-subtopic σ_student (1 − p).
 *   2. Curriculum generation — four subtopics are ordered by
 *      descending σ_student (weakest first, strongest last).
 *      Entry point = first subtopic whose σ_student > τ_entry.
 *   3. Adaptive exercise generation — for the current
 *      subtopic, difficulty ∈ {easy, medium, hard, expert}
 *      tracks σ_student.  σ=0 lifts difficulty; σ=1 drops it.
 *   4. Progress tracking — history of {subtopic, difficulty,
 *      σ_student_pre, σ_student_post} so a learning curve can
 *      be rendered later.
 *   5. σ-honest teaching — if σ_teacher > τ_teacher for a
 *      subtopic, the kernel *abstains* and says so instead of
 *      posturing.
 *
 * v173.0 (this file) ships a deterministic, weights-free
 * session with 4 subtopics (HD vectors, σ-gating, KV cache,
 * v1.58-bit quant), hand-picked σ_teacher per subtopic so one
 * subtopic triggers abstain, and a closed-form adaptive cycle.
 *
 * v173.1 (named, not shipped):
 *   - BitNet-generated diagnostics
 *   - real persona (v132) for tone adaptation
 *   - web-UI learning curves
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V173_TEACH_H
#define COS_V173_TEACH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V173_N_SUBTOPICS   4
#define COS_V173_MAX_EVENTS   16
#define COS_V173_MAX_STR      128

typedef enum {
    COS_V173_DIFF_EASY   = 0,
    COS_V173_DIFF_MEDIUM = 1,
    COS_V173_DIFF_HARD   = 2,
    COS_V173_DIFF_EXPERT = 3,
} cos_v173_difficulty_t;

typedef enum {
    COS_V173_EVT_DIAG      = 0,   /* Socratic diagnostic answer */
    COS_V173_EVT_EXERCISE  = 1,   /* adaptive exercise answer */
    COS_V173_EVT_ABSTAIN   = 2,   /* σ_teacher too high */
    COS_V173_EVT_COMPLETE  = 3,   /* subtopic mastered */
} cos_v173_event_kind_t;

typedef struct {
    char  name[COS_V173_MAX_STR];
    float sigma_teacher;                  /* 1 − confidence to teach this */
    float sigma_student;                  /* current learner uncertainty  */
    int   position;                       /* 0..3 curriculum order */
    bool  mastered;
    cos_v173_difficulty_t difficulty;
} cos_v173_subtopic_t;

typedef struct {
    int                    idx;            /* event ordinal */
    cos_v173_event_kind_t  kind;
    int                    subtopic_idx;
    cos_v173_difficulty_t  difficulty;
    float                  sigma_student_before;
    float                  sigma_student_after;
    float                  student_correctness;  /* in [0,1] */
    char                   note[COS_V173_MAX_STR];
} cos_v173_event_t;

typedef struct {
    char                topic[COS_V173_MAX_STR];
    cos_v173_subtopic_t subs[COS_V173_N_SUBTOPICS];
    int                 order[COS_V173_N_SUBTOPICS]; /* curriculum indices */

    int                 n_events;
    cos_v173_event_t    events[COS_V173_MAX_EVENTS];

    float               tau_teacher;       /* abstain if σ_teacher > this */
    float               tau_entry;         /* first σ_student above this = entry */
    float               tau_mastery;       /* σ_student ≤ this ⇒ mastered */

    uint64_t            seed;
} cos_v173_state_t;

void cos_v173_init(cos_v173_state_t *s, const char *topic, uint64_t seed);

/* Install the canonical 4-subtopic σ-theory fixture. */
void cos_v173_fixture_sigma_theory(cos_v173_state_t *s);

/* Socratic: feed student correctness p ∈ [0,1] for every subtopic
 * in *installation* order; fills σ_student, orders curriculum. */
void cos_v173_socratic(cos_v173_state_t *s, const float *correctness);

/* Adaptive exercise cycle: iterate subtopics in curriculum order,
 * emitting exercises whose difficulty tracks σ_student.  The
 * student performance is simulated deterministically from a
 * per-subtopic learning-rate array and the per-subtopic
 * σ_teacher (low teacher σ ⇒ larger gain).  Emits COMPLETE or
 * ABSTAIN events.  Returns number of events emitted. */
int  cos_v173_teach_cycle(cos_v173_state_t *s);

const char *cos_v173_difficulty_name(cos_v173_difficulty_t d);
const char *cos_v173_event_kind_name(cos_v173_event_kind_t k);

size_t cos_v173_to_json(const cos_v173_state_t *s, char *buf, size_t cap);
size_t cos_v173_trace_ndjson(const cos_v173_state_t *s,
                             char *buf, size_t cap);

int cos_v173_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V173_TEACH_H */
