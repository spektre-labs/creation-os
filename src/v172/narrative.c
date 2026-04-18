/*
 * v172 σ-Narrative — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "narrative.h"

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

/* Add a session with exactly 4 facts and a computed summary. */
static void add_session(cos_v172_state_t *s, int id,
                        const char *label, float sigma,
                        const char *f0, const char *f1,
                        const char *f2, const char *f3,
                        const char *summary) {
    if (s->n_sessions >= COS_V172_MAX_SESSIONS) return;
    cos_v172_session_t *sess = &s->sessions[s->n_sessions++];
    memset(sess, 0, sizeof(*sess));
    sess->id = id;
    copy_str(sess->label,   sizeof(sess->label),   label);
    copy_str(sess->summary, sizeof(sess->summary), summary);
    sess->sigma_coverage = clampf(sigma, 0.0f, 1.0f);
    copy_str(sess->facts[0], COS_V172_MAX_STR, f0);
    copy_str(sess->facts[1], COS_V172_MAX_STR, f1);
    copy_str(sess->facts[2], COS_V172_MAX_STR, f2);
    copy_str(sess->facts[3], COS_V172_MAX_STR, f3);
    sess->n_facts = 4;
}

static void add_goal(cos_v172_state_t *s, const char *name,
                     const char *progress, float sigma, bool done) {
    if (s->n_goals >= COS_V172_MAX_GOALS) return;
    cos_v172_goal_t *g = &s->goals[s->n_goals++];
    memset(g, 0, sizeof(*g));
    copy_str(g->name,     sizeof(g->name),     name);
    copy_str(g->progress, sizeof(g->progress), progress);
    g->sigma_progress = clampf(sigma, 0.0f, 1.0f);
    g->done = done;
}

static void add_person(cos_v172_state_t *s, const char *name,
                       const char *role, int last_sid,
                       const char *ctx, float sigma) {
    if (s->n_people >= COS_V172_MAX_PEOPLE) return;
    cos_v172_person_t *p = &s->people[s->n_people++];
    memset(p, 0, sizeof(*p));
    copy_str(p->name,         sizeof(p->name),         name);
    copy_str(p->role,         sizeof(p->role),         role);
    p->last_session_id = last_sid;
    copy_str(p->last_context, sizeof(p->last_context), ctx);
    p->sigma_ident = clampf(sigma, 0.0f, 1.0f);
}

void cos_v172_fixture_3_sessions(cos_v172_state_t *s) {
    /* Week 1 */
    add_session(s, 0, "week 1", 0.12f,
        "built v84..v111 kernel family",
        "lauri set v1.0 release goal",
        "rene advised on σ-gating",
        "all merge-gates green",
        "week 1: v84..v111 shipped; σ-gating advised by rene");
    /* Week 2 */
    add_session(s, 1, "week 2", 0.18f,
        "built v112..v138 agentic + memory stack",
        "mikko reviewed governance design",
        "cursor ran v115 + v135 tests",
        "goal progress recorded in audit",
        "week 2: v112..v138 shipped; mikko reviewed v167 design");
    /* Week 3 */
    add_session(s, 2, "week 3", 0.22f,
        "built v139..v163 world-intel + self-heal",
        "topias merged the v154 paper draft",
        "cursor ran v164..v168 ecosystem layer",
        "release checklist moved to 80%",
        "week 3: v139..v163 shipped; topias merged v154; v164..v168 staged");

    add_goal(s, "Creation OS v1.0 release",
             "done: v84..v163 + v164..v168 staged; pending: v169..v173",
             0.17f, false);
    add_goal(s, "σ-architecture dissertation",
             "done: thesis scaffold + v154 paper; pending: v6..v173 ablation",
             0.34f, false);
    add_goal(s, "public release under SCSL",
             "done: repo + docs; pending: marketplace publish",
             0.20f, false);

    add_person(s, "lauri",  "owner",    0,
               "set v1.0 release goal",           0.05f);
    add_person(s, "rene",   "advisor",  0,
               "σ-gating guidance",               0.09f);
    add_person(s, "mikko",  "reviewer", 1,
               "reviewed v167 governance design", 0.10f);
    add_person(s, "topias", "editor",   2,
               "merged v154 paper draft",         0.08f);
}

void cos_v172_init(cos_v172_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed         = seed ? seed : 0x172BEEF0172ULL;
    s->tau_coverage = 0.30f;
    cos_v172_fixture_3_sessions(s);
}

/* ------------------------------------------------------------- */
/* Narrative thread                                              */
/* ------------------------------------------------------------- */

size_t cos_v172_narrative_thread(const cos_v172_state_t *s,
                                 char *buf, size_t cap) {
    if (!s || !buf || cap == 0) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used, "Narrative thread:\n");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < s->n_sessions; ++i) {
        const cos_v172_session_t *sess = &s->sessions[i];
        n = snprintf(buf + used, cap - used,
                     "  [%d] %s (σ=%.2f): %s\n",
                     sess->id, sess->label,
                     (double)sess->sigma_coverage, sess->summary);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    return used;
}

/* ------------------------------------------------------------- */
/* Resumption                                                    */
/* ------------------------------------------------------------- */

size_t cos_v172_resume(const cos_v172_state_t *s,
                       char *buf, size_t cap) {
    if (!s || !buf || cap == 0 || s->n_sessions == 0) return 0;
    const cos_v172_session_t *last = &s->sessions[s->n_sessions - 1];

    /* primary in-progress goal = lowest σ_progress among non-done goals */
    const cos_v172_goal_t *primary = NULL;
    float best_sig = 2.0f;
    for (int i = 0; i < s->n_goals; ++i) {
        if (s->goals[i].done) continue;
        if (s->goals[i].sigma_progress < best_sig) {
            primary = &s->goals[i];
            best_sig = s->goals[i].sigma_progress;
        }
    }

    int n = snprintf(buf, cap,
        "Last session: %s (σ=%.2f).  Last progress: %s.  "
        "Active goal: %s — %s (σ=%.2f).  Awaiting instructions.",
        last->label, (double)last->sigma_coverage, last->summary,
        primary ? primary->name     : "(none)",
        primary ? primary->progress : "",
        primary ? (double)primary->sigma_progress : 1.0);
    if (n < 0) return 0;
    return (size_t)n < cap ? (size_t)n : cap - 1;
}

const cos_v172_person_t *
cos_v172_person_get(const cos_v172_state_t *s, const char *name) {
    if (!s || !name) return NULL;
    for (int i = 0; i < s->n_people; ++i)
        if (strcmp(s->people[i].name, name) == 0) return &s->people[i];
    return NULL;
}

/* ------------------------------------------------------------- */
/* JSON                                                          */
/* ------------------------------------------------------------- */

size_t cos_v172_to_json(const cos_v172_state_t *s, char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
                     "{\"kernel\":\"v172\",\"tau_coverage\":%.4f,"
                     "\"n_sessions\":%d,\"n_goals\":%d,\"n_people\":%d,"
                     "\"sessions\":[",
                     (double)s->tau_coverage, s->n_sessions,
                     s->n_goals, s->n_people);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < s->n_sessions; ++i) {
        const cos_v172_session_t *sess = &s->sessions[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"id\":%d,\"label\":\"%s\",\"sigma_coverage\":%.4f,"
            "\"summary\":\"%s\",\"n_facts\":%d,\"facts\":[",
            i == 0 ? "" : ",", sess->id, sess->label,
            (double)sess->sigma_coverage, sess->summary, sess->n_facts);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
        for (int f = 0; f < sess->n_facts; ++f) {
            n = snprintf(buf + used, cap - used,
                         "%s\"%s\"", f == 0 ? "" : ",", sess->facts[f]);
            if (n < 0 || (size_t)n >= cap - used) return 0;
            used += (size_t)n;
        }
        n = snprintf(buf + used, cap - used, "]}");
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "],\"goals\":[");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < s->n_goals; ++i) {
        const cos_v172_goal_t *g = &s->goals[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"name\":\"%s\",\"progress\":\"%s\","
            "\"sigma_progress\":%.4f,\"done\":%s}",
            i == 0 ? "" : ",", g->name, g->progress,
            (double)g->sigma_progress, g->done ? "true" : "false");
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "],\"people\":[");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < s->n_people; ++i) {
        const cos_v172_person_t *p = &s->people[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"name\":\"%s\",\"role\":\"%s\","
            "\"last_session_id\":%d,\"last_context\":\"%s\","
            "\"sigma_ident\":%.4f}",
            i == 0 ? "" : ",", p->name, p->role,
            p->last_session_id, p->last_context,
            (double)p->sigma_ident);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }

    /* embed the deterministic resume line */
    char resume[COS_V172_MAX_RESUME];
    size_t rn = cos_v172_resume(s, resume, sizeof(resume));
    /* escape backslashes/quotes minimally — fixture contains none */
    (void)rn;
    n = snprintf(buf + used, cap - used, "],\"resume\":\"%s\"}", resume);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v172_self_test(void) {
    cos_v172_state_t s;
    cos_v172_init(&s, 0x172FFFFFULL);

    if (s.n_sessions != 3) return 1;
    if (s.n_goals    != 3) return 2;
    if (s.n_people   != 4) return 3;

    for (int i = 0; i < s.n_sessions; ++i) {
        if (s.sessions[i].n_facts != 4) return 4;
        float sc = s.sessions[i].sigma_coverage;
        if (!(sc >= 0.0f && sc <= 1.0f)) return 5;
        if (sc > s.tau_coverage) return 6;    /* all 3 should be ≤ τ */
    }

    /* narrative thread must contain every session label */
    char th[2048];
    size_t thn = cos_v172_narrative_thread(&s, th, sizeof(th));
    if (thn == 0) return 7;
    if (!strstr(th, "week 1")) return 8;
    if (!strstr(th, "week 2")) return 9;
    if (!strstr(th, "week 3")) return 10;

    /* resumption: must reference the last session + primary goal */
    char r[COS_V172_MAX_RESUME];
    size_t rn = cos_v172_resume(&s, r, sizeof(r));
    if (rn == 0) return 11;
    if (!strstr(r, "week 3")) return 12;
    if (!strstr(r, "Creation OS v1.0 release")) return 13;

    /* people lookup */
    const cos_v172_person_t *p = cos_v172_person_get(&s, "lauri");
    if (!p || strcmp(p->role, "owner") != 0) return 14;
    const cos_v172_person_t *q = cos_v172_person_get(&s, "topias");
    if (!q || q->last_session_id != 2) return 15;

    /* determinism */
    cos_v172_state_t a, b;
    cos_v172_init(&a, 0x172FFFFFULL);
    cos_v172_init(&b, 0x172FFFFFULL);
    char A[8192], B[8192];
    size_t na = cos_v172_to_json(&a, A, sizeof(A));
    size_t nb = cos_v172_to_json(&b, B, sizeof(B));
    if (na == 0 || na != nb) return 16;
    if (memcmp(A, B, na) != 0) return 17;

    return 0;
}
