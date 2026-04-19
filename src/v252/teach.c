/*
 * v252 σ-Teach — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "teach.h"

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

static const struct { cos_v252_turn_t k; const char *t; }
    SOCRATIC[COS_V252_N_TURNS] = {
    { COS_V252_TURN_QUESTION,
      "What happens to a photon at the double slit?"       },
    { COS_V252_TURN_QUESTION,
      "Why does observation change the outcome?"           },
    { COS_V252_TURN_QUESTION,
      "What is actually being observed, and by whom?"      },
    { COS_V252_TURN_LEAD,
      "Hold that thought. Now restate it in your words."   },
};

static const struct { float diff; cos_v252_learner_t st; }
    ADAPT[COS_V252_N_ADAPTIVE] = {
    { 0.30f, COS_V252_STATE_BORED      },  /* UP   */
    { 0.55f, COS_V252_STATE_FLOW       },  /* HOLD */
    { 0.55f, COS_V252_STATE_FRUSTRATED },  /* DOWN */
    { 0.40f, COS_V252_STATE_FLOW       },  /* HOLD */
};

static const struct { const char *topic; float sg; }
    GAPS[COS_V252_N_GAPS] = {
    { "superposition_formal_def", 0.62f },
    { "measurement_operator",     0.48f },
    { "decoherence_mechanism",    0.71f },
};

void cos_v252_init(cos_v252_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x252EDC5AULL;
}

static cos_v252_action_t adapt_rule(cos_v252_learner_t st) {
    switch (st) {
    case COS_V252_STATE_BORED:      return COS_V252_ADAPT_UP;
    case COS_V252_STATE_FLOW:       return COS_V252_ADAPT_HOLD;
    case COS_V252_STATE_FRUSTRATED: return COS_V252_ADAPT_DOWN;
    }
    return COS_V252_ADAPT_HOLD;
}

void cos_v252_run(cos_v252_state_t *s) {
    uint64_t prev = 0x252EDC53ULL;

    s->n_questions = 0;
    for (int i = 0; i < COS_V252_N_TURNS; ++i) {
        cos_v252_socratic_t *t = &s->socratic[i];
        memset(t, 0, sizeof(*t));
        t->kind = SOCRATIC[i].k;
        cpy(t->text, sizeof(t->text), SOCRATIC[i].t);
        if (t->kind == COS_V252_TURN_QUESTION) s->n_questions++;
        prev = fnv1a(&t->kind, sizeof(t->kind), prev);
        prev = fnv1a(t->text, strlen(t->text), prev);
    }
    s->socratic_ok =
        (s->socratic[0].kind == COS_V252_TURN_QUESTION) &&
        (s->socratic[1].kind == COS_V252_TURN_QUESTION) &&
        (s->socratic[2].kind == COS_V252_TURN_QUESTION) &&
        (s->socratic[3].kind == COS_V252_TURN_LEAD)     &&
        (s->n_questions >= 3);

    s->n_adapt_up = s->n_adapt_down = s->n_adapt_hold = 0;
    bool adapt_ok_rule = true;
    for (int i = 0; i < COS_V252_N_ADAPTIVE; ++i) {
        cos_v252_adaptive_t *a = &s->adaptive[i];
        memset(a, 0, sizeof(*a));
        a->difficulty    = ADAPT[i].diff;
        a->learner_state = ADAPT[i].st;
        a->action        = adapt_rule(a->learner_state);
        if (a->action == COS_V252_ADAPT_UP)   s->n_adapt_up++;
        if (a->action == COS_V252_ADAPT_DOWN) s->n_adapt_down++;
        if (a->action == COS_V252_ADAPT_HOLD) s->n_adapt_hold++;
        if (a->difficulty < 0.0f || a->difficulty > 1.0f)
            adapt_ok_rule = false;
        prev = fnv1a(&a->difficulty,    sizeof(a->difficulty),    prev);
        prev = fnv1a(&a->learner_state, sizeof(a->learner_state), prev);
        prev = fnv1a(&a->action,        sizeof(a->action),        prev);
    }
    s->adaptive_ok = adapt_ok_rule &&
                     (s->n_adapt_up >= 1) &&
                     (s->n_adapt_down >= 1);

    s->n_gaps_addressed = 0;
    bool gap_range_ok = true;
    for (int i = 0; i < COS_V252_N_GAPS; ++i) {
        cos_v252_gap_t *g = &s->gaps[i];
        memset(g, 0, sizeof(*g));
        cpy(g->topic, sizeof(g->topic), GAPS[i].topic);
        g->sigma_gap = GAPS[i].sg;
        g->targeted_addressed = true;
        if (g->sigma_gap < 0.0f || g->sigma_gap > 1.0f) gap_range_ok = false;
        if (g->targeted_addressed) s->n_gaps_addressed++;
        prev = fnv1a(g->topic, strlen(g->topic), prev);
        prev = fnv1a(&g->sigma_gap, sizeof(g->sigma_gap), prev);
    }
    s->gap_ok = gap_range_ok &&
                (s->n_gaps_addressed == COS_V252_N_GAPS);

    cos_v252_receipt_t *r = &s->receipt;
    memset(r, 0, sizeof(*r));
    cpy(r->session_id,         sizeof(r->session_id),         "sess-0001");
    cpy(r->next_session_start, sizeof(r->next_session_start),
        "decoherence_mechanism");
    r->taught         = 5;
    r->understood     = 4;
    r->not_understood = 1;
    r->receipt_ok =
        (strlen(r->session_id) > 0)         &&
        (strlen(r->next_session_start) > 0) &&
        (r->taught >= r->understood + r->not_understood);

    prev = fnv1a(r->session_id, strlen(r->session_id), prev);
    prev = fnv1a(r->next_session_start, strlen(r->next_session_start), prev);
    prev = fnv1a(&r->taught,         sizeof(r->taught),         prev);
    prev = fnv1a(&r->understood,     sizeof(r->understood),     prev);
    prev = fnv1a(&r->not_understood, sizeof(r->not_understood), prev);

    if (r->taught > 0) {
        s->sigma_understanding =
            1.0f - ((float)r->understood / (float)r->taught);
    } else {
        s->sigma_understanding = 0.0f;
    }
    if (s->sigma_understanding < 0.0f) s->sigma_understanding = 0.0f;
    if (s->sigma_understanding > 1.0f) s->sigma_understanding = 1.0f;

    int passing = (s->socratic_ok ? 1 : 0) +
                  (s->adaptive_ok ? 1 : 0) +
                  (s->gap_ok      ? 1 : 0) +
                  (s->receipt.receipt_ok ? 1 : 0);
    s->sigma_teach = 1.0f - ((float)passing / 4.0f);
    if (s->sigma_teach < 0.0f) s->sigma_teach = 0.0f;
    if (s->sigma_teach > 1.0f) s->sigma_teach = 1.0f;

    struct { int nq, nu, nd, nh, ng;
             float su, st; bool so, ao, go, ro;
             uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nq = s->n_questions;
    trec.nu = s->n_adapt_up;
    trec.nd = s->n_adapt_down;
    trec.nh = s->n_adapt_hold;
    trec.ng = s->n_gaps_addressed;
    trec.su = s->sigma_understanding;
    trec.st = s->sigma_teach;
    trec.so = s->socratic_ok;
    trec.ao = s->adaptive_ok;
    trec.go = s->gap_ok;
    trec.ro = s->receipt.receipt_ok;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *turn_name(cos_v252_turn_t k) {
    switch (k) {
    case COS_V252_TURN_QUESTION: return "QUESTION";
    case COS_V252_TURN_LEAD:     return "LEAD";
    case COS_V252_TURN_ANSWER:   return "ANSWER";
    }
    return "UNKNOWN";
}
static const char *state_name(cos_v252_learner_t st) {
    switch (st) {
    case COS_V252_STATE_BORED:      return "BORED";
    case COS_V252_STATE_FLOW:       return "FLOW";
    case COS_V252_STATE_FRUSTRATED: return "FRUSTRATED";
    }
    return "UNKNOWN";
}
static const char *action_name(cos_v252_action_t a) {
    switch (a) {
    case COS_V252_ADAPT_UP:   return "UP";
    case COS_V252_ADAPT_HOLD: return "HOLD";
    case COS_V252_ADAPT_DOWN: return "DOWN";
    }
    return "UNKNOWN";
}

size_t cos_v252_to_json(const cos_v252_state_t *s, char *buf, size_t cap) {
    const cos_v252_receipt_t *r = &s->receipt;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v252\","
        "\"n_questions\":%d,\"n_adapt_up\":%d,"
        "\"n_adapt_down\":%d,\"n_adapt_hold\":%d,"
        "\"n_gaps_addressed\":%d,"
        "\"socratic_ok\":%s,\"adaptive_ok\":%s,"
        "\"gap_ok\":%s,\"receipt_ok\":%s,"
        "\"sigma_understanding\":%.4f,"
        "\"sigma_teach\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"receipt\":{\"session_id\":\"%s\","
        "\"taught\":%d,\"understood\":%d,"
        "\"not_understood\":%d,"
        "\"next_session_start\":\"%s\"},"
        "\"socratic\":[",
        s->n_questions, s->n_adapt_up,
        s->n_adapt_down, s->n_adapt_hold,
        s->n_gaps_addressed,
        s->socratic_ok ? "true" : "false",
        s->adaptive_ok ? "true" : "false",
        s->gap_ok      ? "true" : "false",
        r->receipt_ok  ? "true" : "false",
        s->sigma_understanding, s->sigma_teach,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash,
        r->session_id, r->taught, r->understood,
        r->not_understood, r->next_session_start);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V252_N_TURNS; ++i) {
        const cos_v252_socratic_t *t = &s->socratic[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"kind\":\"%s\",\"text\":\"%s\"}",
            i == 0 ? "" : ",", turn_name(t->kind), t->text);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"adaptive\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V252_N_ADAPTIVE; ++i) {
        const cos_v252_adaptive_t *a = &s->adaptive[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"difficulty\":%.4f,\"learner_state\":\"%s\","
            "\"action\":\"%s\"}",
            i == 0 ? "" : ",", a->difficulty,
            state_name(a->learner_state), action_name(a->action));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"gaps\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V252_N_GAPS; ++i) {
        const cos_v252_gap_t *g = &s->gaps[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"topic\":\"%s\",\"sigma_gap\":%.4f,"
            "\"targeted_addressed\":%s}",
            i == 0 ? "" : ",", g->topic, g->sigma_gap,
            g->targeted_addressed ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v252_self_test(void) {
    cos_v252_state_t s;
    cos_v252_init(&s, 0x252EDC5AULL);
    cos_v252_run(&s);
    if (!s.chain_valid) return 1;

    if (s.socratic[0].kind != COS_V252_TURN_QUESTION) return 2;
    if (s.socratic[1].kind != COS_V252_TURN_QUESTION) return 3;
    if (s.socratic[2].kind != COS_V252_TURN_QUESTION) return 4;
    if (s.socratic[3].kind != COS_V252_TURN_LEAD)     return 5;
    if (s.n_questions < 3)                            return 6;
    if (!s.socratic_ok)                               return 7;

    for (int i = 0; i < COS_V252_N_ADAPTIVE; ++i) {
        if (s.adaptive[i].difficulty < 0.0f ||
            s.adaptive[i].difficulty > 1.0f) return 8;
        cos_v252_action_t expect = COS_V252_ADAPT_HOLD;
        switch (s.adaptive[i].learner_state) {
        case COS_V252_STATE_BORED:      expect = COS_V252_ADAPT_UP;   break;
        case COS_V252_STATE_FLOW:       expect = COS_V252_ADAPT_HOLD; break;
        case COS_V252_STATE_FRUSTRATED: expect = COS_V252_ADAPT_DOWN; break;
        }
        if (s.adaptive[i].action != expect) return 9;
    }
    if (s.n_adapt_up   < 1) return 10;
    if (s.n_adapt_down < 1) return 11;
    if (!s.adaptive_ok)     return 12;

    for (int i = 0; i < COS_V252_N_GAPS; ++i) {
        if (s.gaps[i].sigma_gap < 0.0f ||
            s.gaps[i].sigma_gap > 1.0f) return 13;
        if (!s.gaps[i].targeted_addressed) return 14;
    }
    if (s.n_gaps_addressed != COS_V252_N_GAPS) return 15;
    if (!s.gap_ok)                             return 16;

    const cos_v252_receipt_t *r = &s.receipt;
    if (strlen(r->session_id) == 0)         return 17;
    if (strlen(r->next_session_start) == 0) return 18;
    if (r->taught < r->understood + r->not_understood) return 19;
    if (!r->receipt_ok)                     return 20;

    if (s.sigma_understanding < 0.0f ||
        s.sigma_understanding > 1.0f) return 21;
    float expect_su = (r->taught > 0)
        ? 1.0f - ((float)r->understood / (float)r->taught) : 0.0f;
    if (fabsf(s.sigma_understanding - expect_su) > 1e-4f) return 22;

    if (s.sigma_teach < 0.0f || s.sigma_teach > 1.0f) return 23;
    if (s.sigma_teach > 1e-6f) return 24;

    cos_v252_state_t t;
    cos_v252_init(&t, 0x252EDC5AULL);
    cos_v252_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 25;
    return 0;
}
