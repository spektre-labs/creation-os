/*
 * v173 σ-Teach — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "teach.h"

#include <stdio.h>
#include <string.h>

static void copy_str(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

const char *cos_v173_difficulty_name(cos_v173_difficulty_t d) {
    switch (d) {
        case COS_V173_DIFF_EASY:   return "easy";
        case COS_V173_DIFF_MEDIUM: return "medium";
        case COS_V173_DIFF_HARD:   return "hard";
        case COS_V173_DIFF_EXPERT: return "expert";
        default:                   return "?";
    }
}

const char *cos_v173_event_kind_name(cos_v173_event_kind_t k) {
    switch (k) {
        case COS_V173_EVT_DIAG:     return "diag";
        case COS_V173_EVT_EXERCISE: return "exercise";
        case COS_V173_EVT_ABSTAIN:  return "abstain";
        case COS_V173_EVT_COMPLETE: return "complete";
        default:                    return "?";
    }
}

void cos_v173_init(cos_v173_state_t *s, const char *topic, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x173EEEE0173ULL;
    s->tau_teacher = 0.60f;   /* teacher abstains above this */
    s->tau_entry   = 0.30f;   /* weakest subtopic above this */
    s->tau_mastery = 0.20f;   /* subtopic mastered at or below this */
    copy_str(s->topic, sizeof(s->topic), topic ? topic : "σ-theory");
}

void cos_v173_fixture_sigma_theory(cos_v173_state_t *s) {
    /* 4 canonical subtopics. σ_teacher hand-picked so the
     * "v1.58-bit quantization" subtopic triggers abstain. */
    struct { const char *name; float st; } raw[COS_V173_N_SUBTOPICS] = {
        { "HD vectors",             0.10f },
        { "σ-gating",               0.08f },
        { "KV cache discipline",    0.18f },
        { "v1.58-bit quantization", 0.75f },   /* > τ_teacher → abstain */
    };
    for (int i = 0; i < COS_V173_N_SUBTOPICS; ++i) {
        cos_v173_subtopic_t *sub = &s->subs[i];
        memset(sub, 0, sizeof(*sub));
        copy_str(sub->name, sizeof(sub->name), raw[i].name);
        sub->sigma_teacher = raw[i].st;
        sub->sigma_student = 1.0f;   /* unknown until diagnostic */
        sub->position      = i;
        sub->mastered      = false;
        sub->difficulty    = COS_V173_DIFF_EASY;
    }
    /* initial order = installation order (replaced by Socratic) */
    for (int i = 0; i < COS_V173_N_SUBTOPICS; ++i) s->order[i] = i;
}

/* ------------------------------------------------------------- */
/* Socratic                                                      */
/* ------------------------------------------------------------- */

static cos_v173_difficulty_t difficulty_for(float sigma_student) {
    if (sigma_student <= 0.20f) return COS_V173_DIFF_EXPERT;
    if (sigma_student <= 0.40f) return COS_V173_DIFF_HARD;
    if (sigma_student <= 0.65f) return COS_V173_DIFF_MEDIUM;
    return COS_V173_DIFF_EASY;
}

void cos_v173_socratic(cos_v173_state_t *s, const float *correctness) {
    /* Record a diag event per subtopic and set σ_student = 1 − p. */
    for (int i = 0; i < COS_V173_N_SUBTOPICS; ++i) {
        float p = clampf(correctness[i], 0.0f, 1.0f);
        s->subs[i].sigma_student = 1.0f - p;
        s->subs[i].difficulty    = difficulty_for(s->subs[i].sigma_student);

        if (s->n_events < COS_V173_MAX_EVENTS) {
            cos_v173_event_t *e = &s->events[s->n_events];
            memset(e, 0, sizeof(*e));
            e->idx                  = s->n_events;
            e->kind                 = COS_V173_EVT_DIAG;
            e->subtopic_idx         = i;
            e->difficulty           = s->subs[i].difficulty;
            e->sigma_student_before = 1.0f;
            e->sigma_student_after  = s->subs[i].sigma_student;
            e->student_correctness  = p;
            snprintf(e->note, sizeof(e->note),
                     "diagnostic %s: p=%.2f σ_student=%.2f",
                     s->subs[i].name, (double)p,
                     (double)s->subs[i].sigma_student);
            s->n_events++;
        }
    }
    /* curriculum: sort indices by σ_student DESCENDING (weak first). */
    int idx[COS_V173_N_SUBTOPICS];
    for (int i = 0; i < COS_V173_N_SUBTOPICS; ++i) idx[i] = i;
    for (int i = 0; i < COS_V173_N_SUBTOPICS; ++i)
        for (int j = i + 1; j < COS_V173_N_SUBTOPICS; ++j)
            if (s->subs[idx[j]].sigma_student > s->subs[idx[i]].sigma_student) {
                int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
            }
    for (int i = 0; i < COS_V173_N_SUBTOPICS; ++i) s->order[i] = idx[i];
}

/* ------------------------------------------------------------- */
/* Teach cycle                                                   */
/* ------------------------------------------------------------- */

int cos_v173_teach_cycle(cos_v173_state_t *s) {
    int emitted = 0;
    /* Walk curriculum; skip abstained subtopics. */
    for (int k = 0; k < COS_V173_N_SUBTOPICS; ++k) {
        int i = s->order[k];
        cos_v173_subtopic_t *sub = &s->subs[i];

        if (sub->sigma_teacher > s->tau_teacher) {
            if (s->n_events < COS_V173_MAX_EVENTS) {
                cos_v173_event_t *e = &s->events[s->n_events];
                memset(e, 0, sizeof(*e));
                e->idx                  = s->n_events;
                e->kind                 = COS_V173_EVT_ABSTAIN;
                e->subtopic_idx         = i;
                e->difficulty           = sub->difficulty;
                e->sigma_student_before = sub->sigma_student;
                e->sigma_student_after  = sub->sigma_student;
                e->student_correctness  = 0.0f;
                snprintf(e->note, sizeof(e->note),
                         "ABSTAIN %s: σ_teacher=%.2f > τ_teacher=%.2f; "
                         "verify with another source",
                         sub->name, (double)sub->sigma_teacher,
                         (double)s->tau_teacher);
                s->n_events++; emitted++;
            }
            continue;
        }

        /* Up to 2 exercises per subtopic; student improves
         * deterministically: σ_after = max(τ_mastery,
         *   σ_before · (1 − (0.5 − σ_teacher))). */
        for (int round = 0; round < 2; ++round) {
            float before = sub->sigma_student;
            float gain   = 0.5f - sub->sigma_teacher;      /* strong teacher ⇒ big gain */
            if (gain < 0.15f) gain = 0.15f;
            float after  = before * (1.0f - gain);
            if (after < s->tau_mastery) after = s->tau_mastery;
            sub->sigma_student = after;
            sub->difficulty    = difficulty_for(after);
            float correctness  = 1.0f - after;

            if (s->n_events < COS_V173_MAX_EVENTS) {
                cos_v173_event_t *e = &s->events[s->n_events];
                memset(e, 0, sizeof(*e));
                e->idx                  = s->n_events;
                e->kind                 = COS_V173_EVT_EXERCISE;
                e->subtopic_idx         = i;
                e->difficulty           = sub->difficulty;
                e->sigma_student_before = before;
                e->sigma_student_after  = after;
                e->student_correctness  = correctness;
                snprintf(e->note, sizeof(e->note),
                         "exercise %s (%s): σ %.2f → %.2f",
                         sub->name,
                         cos_v173_difficulty_name(sub->difficulty),
                         (double)before, (double)after);
                s->n_events++; emitted++;
            }

            if (after <= s->tau_mastery) break;
        }

        if (sub->sigma_student <= s->tau_mastery) {
            sub->mastered = true;
            if (s->n_events < COS_V173_MAX_EVENTS) {
                cos_v173_event_t *e = &s->events[s->n_events];
                memset(e, 0, sizeof(*e));
                e->idx                  = s->n_events;
                e->kind                 = COS_V173_EVT_COMPLETE;
                e->subtopic_idx         = i;
                e->difficulty           = sub->difficulty;
                e->sigma_student_before = sub->sigma_student;
                e->sigma_student_after  = sub->sigma_student;
                e->student_correctness  = 1.0f - sub->sigma_student;
                snprintf(e->note, sizeof(e->note),
                         "COMPLETE %s: mastered at σ=%.2f",
                         sub->name, (double)sub->sigma_student);
                s->n_events++; emitted++;
            }
        }
    }
    return emitted;
}

/* ------------------------------------------------------------- */
/* JSON / NDJSON                                                 */
/* ------------------------------------------------------------- */

size_t cos_v173_to_json(const cos_v173_state_t *s, char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
        "{\"kernel\":\"v173\",\"topic\":\"%s\","
        "\"tau_teacher\":%.4f,\"tau_entry\":%.4f,"
        "\"tau_mastery\":%.4f,\"subtopics\":[",
        s->topic, (double)s->tau_teacher,
        (double)s->tau_entry, (double)s->tau_mastery);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < COS_V173_N_SUBTOPICS; ++i) {
        const cos_v173_subtopic_t *sub = &s->subs[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"name\":\"%s\",\"sigma_teacher\":%.4f,"
            "\"sigma_student\":%.4f,\"difficulty\":\"%s\","
            "\"position\":%d,\"mastered\":%s}",
            i == 0 ? "" : ",",
            sub->name, (double)sub->sigma_teacher,
            (double)sub->sigma_student,
            cos_v173_difficulty_name(sub->difficulty),
            sub->position, sub->mastered ? "true" : "false");
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "],\"order\":[");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < COS_V173_N_SUBTOPICS; ++i) {
        n = snprintf(buf + used, cap - used, "%s%d",
                     i == 0 ? "" : ",", s->order[i]);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "],\"n_events\":%d}", s->n_events);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

size_t cos_v173_trace_ndjson(const cos_v173_state_t *s,
                             char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    for (int i = 0; i < s->n_events; ++i) {
        const cos_v173_event_t *e = &s->events[i];
        int n = snprintf(buf + used, cap - used,
            "{\"kernel\":\"v173\",\"idx\":%d,\"event\":\"%s\","
            "\"subtopic\":\"%s\",\"difficulty\":\"%s\","
            "\"sigma_student_before\":%.4f,\"sigma_student_after\":%.4f,"
            "\"student_correctness\":%.4f,\"note\":\"%s\"}\n",
            e->idx, cos_v173_event_kind_name(e->kind),
            s->subs[e->subtopic_idx].name,
            cos_v173_difficulty_name(e->difficulty),
            (double)e->sigma_student_before,
            (double)e->sigma_student_after,
            (double)e->student_correctness, e->note);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v173_self_test(void) {
    cos_v173_state_t s;
    cos_v173_init(&s, "σ-theory", 0x173FACE0173ULL);
    cos_v173_fixture_sigma_theory(&s);

    /* Learner knows HD vectors well, σ-gating medium,
     * KV cache poorly, v1.58-bit barely. */
    float diagnostic[COS_V173_N_SUBTOPICS] = {
        0.80f,   /* HD vectors */
        0.55f,   /* σ-gating */
        0.25f,   /* KV cache */
        0.10f,   /* v1.58-bit */
    };
    cos_v173_socratic(&s, diagnostic);

    /* Diagnostic events emitted */
    if (s.n_events != COS_V173_N_SUBTOPICS) return 1;

    /* σ_student ordering: weakest should come first in curriculum. */
    float sigmas[COS_V173_N_SUBTOPICS];
    for (int i = 0; i < COS_V173_N_SUBTOPICS; ++i)
        sigmas[i] = s.subs[s.order[i]].sigma_student;
    for (int i = 1; i < COS_V173_N_SUBTOPICS; ++i)
        if (sigmas[i] > sigmas[i - 1]) return 2;   /* must be descending */

    /* difficulty: weakest subtopic (KV cache? or v1.58-bit) should be easy;
     * strongest (HD vectors, σ_student=0.20) should be expert. */
    int hd = -1;
    for (int i = 0; i < COS_V173_N_SUBTOPICS; ++i)
        if (strcmp(s.subs[i].name, "HD vectors") == 0) hd = i;
    if (hd < 0) return 3;
    if (s.subs[hd].difficulty != COS_V173_DIFF_EXPERT) return 4;

    int teach_events = cos_v173_teach_cycle(&s);
    if (teach_events <= 0) return 5;

    /* v1.58-bit quantization must trigger ABSTAIN. */
    int quant = -1;
    for (int i = 0; i < COS_V173_N_SUBTOPICS; ++i)
        if (strcmp(s.subs[i].name, "v1.58-bit quantization") == 0) quant = i;
    if (quant < 0) return 6;

    bool saw_abstain = false;
    for (int i = 0; i < s.n_events; ++i) {
        if (s.events[i].subtopic_idx == quant &&
            s.events[i].kind == COS_V173_EVT_ABSTAIN)
            saw_abstain = true;
    }
    if (!saw_abstain) return 7;

    /* At least one subtopic must have mastered after the cycle. */
    int n_mastered = 0;
    for (int i = 0; i < COS_V173_N_SUBTOPICS; ++i)
        if (s.subs[i].mastered) n_mastered++;
    if (n_mastered == 0) return 8;

    /* All non-abstained exercises should reduce σ_student. */
    for (int i = 0; i < s.n_events; ++i) {
        if (s.events[i].kind != COS_V173_EVT_EXERCISE) continue;
        if (!(s.events[i].sigma_student_after <=
              s.events[i].sigma_student_before + 1e-6f)) return 9;
    }

    /* Determinism */
    cos_v173_state_t a, b;
    cos_v173_init(&a, "σ-theory", 0x173FACE0173ULL);
    cos_v173_init(&b, "σ-theory", 0x173FACE0173ULL);
    cos_v173_fixture_sigma_theory(&a);
    cos_v173_fixture_sigma_theory(&b);
    cos_v173_socratic(&a, diagnostic);
    cos_v173_socratic(&b, diagnostic);
    cos_v173_teach_cycle(&a);
    cos_v173_teach_cycle(&b);
    char A[8192], B[8192];
    size_t na = cos_v173_to_json(&a, A, sizeof(A));
    size_t nb = cos_v173_to_json(&b, B, sizeof(B));
    if (na == 0 || na != nb) return 10;
    if (memcmp(A, B, na) != 0) return 11;

    return 0;
}
