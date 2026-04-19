/*
 * v254 σ-Tutor — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "tutor.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

static const struct { const char *n; float pm; float sm; }
    SKILLS[COS_V254_N_SKILLS] = {
    { "linear_algebra", 0.78f, 0.12f },
    { "calculus",       0.62f, 0.18f },
    { "probability",    0.54f, 0.21f },
    { "discrete_math",  0.71f, 0.15f },
};

static const struct { const char *sk; float diff; float lvl; }
    EXERCISES[COS_V254_N_EXERCISES] = {
    { "linear_algebra", 0.80f, 0.78f },   /* |Δ|=0.02 */
    { "calculus",       0.68f, 0.62f },   /* |Δ|=0.06 */
    { "probability",    0.60f, 0.54f },   /* |Δ|=0.06 */
    { "discrete_math",  0.75f, 0.71f },   /* |Δ|=0.04 */
};

static const struct { const char *n; float sf; }
    MODALITIES[COS_V254_N_MODALITIES] = {
    { "text",       0.31f },
    { "code",       0.14f },   /* minimum → chosen */
    { "simulation", 0.27f },
    { "dialog",     0.42f },
};

static const struct { const char *sk; int b; int a; }
    PROGRESS[COS_V254_N_PROGRESS] = {
    { "linear_algebra", 40, 78 },
    { "calculus",       55, 62 },
    { "probability",    50, 54 },
};

static const char *PRIVACY[COS_V254_N_PRIVACY] = {
    "local_only","no_third_party","user_owned_data","export_supported"
};

void cos_v254_init(cos_v254_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed    = seed ? seed : 0x254ADAFEULL;
    s->tau_fit = 0.20f;
}

void cos_v254_run(cos_v254_state_t *s) {
    uint64_t prev = 0x254ADAF0ULL;

    s->n_skills_ok = 0;
    for (int i = 0; i < COS_V254_N_SKILLS; ++i) {
        cos_v254_skill_t *k = &s->skills[i];
        memset(k, 0, sizeof(*k));
        cpy(k->name, sizeof(k->name), SKILLS[i].n);
        k->p_mastery     = SKILLS[i].pm;
        k->sigma_mastery = SKILLS[i].sm;
        k->skill_ok =
            (k->p_mastery     >= 0.0f && k->p_mastery     <= 1.0f) &&
            (k->sigma_mastery >= 0.0f && k->sigma_mastery <= 1.0f);
        if (k->skill_ok) s->n_skills_ok++;
        prev = fnv1a(k->name, strlen(k->name), prev);
        prev = fnv1a(&k->p_mastery,     sizeof(k->p_mastery),     prev);
        prev = fnv1a(&k->sigma_mastery, sizeof(k->sigma_mastery), prev);
    }

    s->n_exercises_fit = 0;
    for (int i = 0; i < COS_V254_N_EXERCISES; ++i) {
        cos_v254_exercise_t *e = &s->exercises[i];
        memset(e, 0, sizeof(*e));
        cpy(e->skill, sizeof(e->skill), EXERCISES[i].sk);
        e->difficulty     = EXERCISES[i].diff;
        e->learner_level  = EXERCISES[i].lvl;
        float d = e->difficulty - e->learner_level;
        e->sigma_difficulty = d < 0 ? -d : d;
        e->fit = (e->sigma_difficulty <= s->tau_fit);
        if (e->fit) s->n_exercises_fit++;
        prev = fnv1a(e->skill, strlen(e->skill), prev);
        prev = fnv1a(&e->difficulty,       sizeof(e->difficulty),       prev);
        prev = fnv1a(&e->learner_level,    sizeof(e->learner_level),    prev);
        prev = fnv1a(&e->sigma_difficulty, sizeof(e->sigma_difficulty), prev);
    }

    int min_idx = 0;
    for (int i = 1; i < COS_V254_N_MODALITIES; ++i)
        if (MODALITIES[i].sf < MODALITIES[min_idx].sf) min_idx = i;
    s->n_chosen_modalities = 0;
    for (int i = 0; i < COS_V254_N_MODALITIES; ++i) {
        cos_v254_modality_t *m = &s->modalities[i];
        memset(m, 0, sizeof(*m));
        cpy(m->name, sizeof(m->name), MODALITIES[i].n);
        m->sigma_fit = MODALITIES[i].sf;
        m->chosen    = (i == min_idx);
        if (m->chosen) s->n_chosen_modalities++;
        prev = fnv1a(m->name, strlen(m->name), prev);
        prev = fnv1a(&m->sigma_fit, sizeof(m->sigma_fit), prev);
        prev = fnv1a(&m->chosen,    sizeof(m->chosen),    prev);
    }

    s->n_positive_progress = 0;
    bool all_nonneg = true;
    for (int i = 0; i < COS_V254_N_PROGRESS; ++i) {
        cos_v254_progress_t *p = &s->progress[i];
        memset(p, 0, sizeof(*p));
        cpy(p->skill, sizeof(p->skill), PROGRESS[i].sk);
        p->pct_before = PROGRESS[i].b;
        p->pct_after  = PROGRESS[i].a;
        p->delta_pct  = p->pct_after - p->pct_before;
        if (p->pct_after < p->pct_before) all_nonneg = false;
        if (p->delta_pct > 0) s->n_positive_progress++;
        prev = fnv1a(p->skill, strlen(p->skill), prev);
        prev = fnv1a(&p->pct_before, sizeof(p->pct_before), prev);
        prev = fnv1a(&p->pct_after,  sizeof(p->pct_after),  prev);
    }
    int progress_rows_ok = all_nonneg ? COS_V254_N_PROGRESS : 0;

    s->n_privacy_enabled = 0;
    for (int i = 0; i < COS_V254_N_PRIVACY; ++i) {
        cos_v254_privacy_t *p = &s->privacy[i];
        memset(p, 0, sizeof(*p));
        cpy(p->flag, sizeof(p->flag), PRIVACY[i]);
        p->enabled = true;
        if (p->enabled) s->n_privacy_enabled++;
        prev = fnv1a(p->flag, strlen(p->flag), prev);
    }

    int total   = COS_V254_N_SKILLS + COS_V254_N_EXERCISES +
                  1 + COS_V254_N_PROGRESS + COS_V254_N_PRIVACY;
    int passing = s->n_skills_ok + s->n_exercises_fit +
                  (s->n_chosen_modalities == 1 ? 1 : 0) +
                  progress_rows_ok + s->n_privacy_enabled;
    s->sigma_tutor = 1.0f - ((float)passing / (float)total);
    if (s->sigma_tutor < 0.0f) s->sigma_tutor = 0.0f;
    if (s->sigma_tutor > 1.0f) s->sigma_tutor = 1.0f;

    struct { int sk, ex, mo, pr, pv; float st, tf;
             uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.sk = s->n_skills_ok;
    trec.ex = s->n_exercises_fit;
    trec.mo = s->n_chosen_modalities;
    trec.pr = s->n_positive_progress;
    trec.pv = s->n_privacy_enabled;
    trec.st = s->sigma_tutor;
    trec.tf = s->tau_fit;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v254_to_json(const cos_v254_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v254\","
        "\"tau_fit\":%.3f,"
        "\"n_skills\":%d,\"n_exercises\":%d,"
        "\"n_modalities\":%d,\"n_progress\":%d,"
        "\"n_privacy\":%d,"
        "\"n_skills_ok\":%d,\"n_exercises_fit\":%d,"
        "\"n_chosen_modalities\":%d,"
        "\"n_positive_progress\":%d,\"n_privacy_enabled\":%d,"
        "\"sigma_tutor\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"skills\":[",
        s->tau_fit,
        COS_V254_N_SKILLS, COS_V254_N_EXERCISES,
        COS_V254_N_MODALITIES, COS_V254_N_PROGRESS,
        COS_V254_N_PRIVACY,
        s->n_skills_ok, s->n_exercises_fit,
        s->n_chosen_modalities,
        s->n_positive_progress, s->n_privacy_enabled,
        s->sigma_tutor,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V254_N_SKILLS; ++i) {
        const cos_v254_skill_t *k = &s->skills[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"p_mastery\":%.4f,"
            "\"sigma_mastery\":%.4f,\"skill_ok\":%s}",
            i == 0 ? "" : ",", k->name,
            k->p_mastery, k->sigma_mastery,
            k->skill_ok ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"exercises\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V254_N_EXERCISES; ++i) {
        const cos_v254_exercise_t *e = &s->exercises[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"skill\":\"%s\",\"difficulty\":%.4f,"
            "\"learner_level\":%.4f,\"sigma_difficulty\":%.4f,"
            "\"fit\":%s}",
            i == 0 ? "" : ",", e->skill,
            e->difficulty, e->learner_level, e->sigma_difficulty,
            e->fit ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"modalities\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V254_N_MODALITIES; ++i) {
        const cos_v254_modality_t *m = &s->modalities[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"sigma_fit\":%.4f,"
            "\"chosen\":%s}",
            i == 0 ? "" : ",", m->name, m->sigma_fit,
            m->chosen ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"progress\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V254_N_PROGRESS; ++i) {
        const cos_v254_progress_t *p = &s->progress[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"skill\":\"%s\",\"pct_before\":%d,"
            "\"pct_after\":%d,\"delta_pct\":%d}",
            i == 0 ? "" : ",", p->skill,
            p->pct_before, p->pct_after, p->delta_pct);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"privacy\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V254_N_PRIVACY; ++i) {
        const cos_v254_privacy_t *p = &s->privacy[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"flag\":\"%s\",\"enabled\":%s}",
            i == 0 ? "" : ",", p->flag,
            p->enabled ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v254_self_test(void) {
    cos_v254_state_t s;
    cos_v254_init(&s, 0x254ADAFEULL);
    cos_v254_run(&s);
    if (!s.chain_valid) return 1;

    const char *sn[COS_V254_N_SKILLS] = {
        "linear_algebra","calculus","probability","discrete_math"
    };
    for (int i = 0; i < COS_V254_N_SKILLS; ++i) {
        if (strcmp(s.skills[i].name, sn[i]) != 0) return 2;
        if (s.skills[i].p_mastery < 0.0f || s.skills[i].p_mastery > 1.0f) return 3;
        if (s.skills[i].sigma_mastery < 0.0f ||
            s.skills[i].sigma_mastery > 1.0f) return 4;
        if (!s.skills[i].skill_ok) return 5;
    }
    if (s.n_skills_ok != COS_V254_N_SKILLS) return 6;

    for (int i = 0; i < COS_V254_N_EXERCISES; ++i) {
        if (s.exercises[i].sigma_difficulty > s.tau_fit) return 7;
        if (!s.exercises[i].fit) return 8;
        float d = s.exercises[i].difficulty -
                  s.exercises[i].learner_level;
        float ad = d < 0 ? -d : d;
        if (fabsf(s.exercises[i].sigma_difficulty - ad) > 1e-6f) return 9;
    }
    if (s.n_exercises_fit != COS_V254_N_EXERCISES) return 10;

    const char *mn[COS_V254_N_MODALITIES] = {
        "text","code","simulation","dialog"
    };
    int chosen_count = 0, chosen_idx = -1;
    float min_sf = 1.1f;
    int min_i = -1;
    for (int i = 0; i < COS_V254_N_MODALITIES; ++i) {
        if (strcmp(s.modalities[i].name, mn[i]) != 0) return 11;
        if (s.modalities[i].sigma_fit < 0.0f ||
            s.modalities[i].sigma_fit > 1.0f) return 12;
        if (s.modalities[i].sigma_fit < min_sf) {
            min_sf = s.modalities[i].sigma_fit; min_i = i;
        }
        if (s.modalities[i].chosen) { chosen_count++; chosen_idx = i; }
    }
    if (chosen_count != 1)          return 13;
    if (chosen_idx   != min_i)      return 14;

    bool some_positive = false;
    for (int i = 0; i < COS_V254_N_PROGRESS; ++i) {
        if (s.progress[i].pct_after < s.progress[i].pct_before) return 15;
        if (s.progress[i].delta_pct > 0) some_positive = true;
        if (s.progress[i].delta_pct !=
            s.progress[i].pct_after - s.progress[i].pct_before) return 16;
    }
    if (!some_positive) return 17;

    const char *pn[COS_V254_N_PRIVACY] = {
        "local_only","no_third_party","user_owned_data","export_supported"
    };
    for (int i = 0; i < COS_V254_N_PRIVACY; ++i) {
        if (strcmp(s.privacy[i].flag, pn[i]) != 0) return 18;
        if (!s.privacy[i].enabled) return 19;
    }
    if (s.n_privacy_enabled != COS_V254_N_PRIVACY) return 20;

    if (s.sigma_tutor < 0.0f || s.sigma_tutor > 1.0f) return 21;
    if (s.sigma_tutor > 1e-6f) return 22;

    cos_v254_state_t t;
    cos_v254_init(&t, 0x254ADAFEULL);
    cos_v254_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 23;
    return 0;
}
