/*
 * v236 σ-Autobiography — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "autobiography.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy_name(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

typedef struct {
    cos_v236_kind_t  kind;
    int              tick;
    float            sigma;
    const char      *domain;
    const char      *content;
    bool             consistent_with_prev;
} cos_v236_fx_t;

static const cos_v236_fx_t FIX[COS_V236_N_MILESTONES] = {
    { COS_V236_KIND_FIRST_SIGMA_BELOW_0_10, 120, 0.08f,
      "maths",    "sigma=0.09 on maths proof",           true },
    { COS_V236_KIND_FIRST_SUCCESSFUL_RSI,   210, 0.12f,
      "sigma",    "v148 RSI loop converged",             true },
    { COS_V236_KIND_FIRST_FORK,             310, 0.05f,
      "meta",     "fork-1 spawned from main",            true },
    { COS_V236_KIND_LARGEST_IMPROVEMENT,    420, 0.14f,
      "retrieve", "sigma dropped 0.42 to 0.11",          true },
    { COS_V236_KIND_NEW_SKILL_ACQUIRED,     505, 0.18f,
      "code",     "wrote first v0 kernel unassisted",    true },
    { COS_V236_KIND_FIRST_ABSTENTION,       580, 0.22f,
      "biology",  "sigma above tau, refused to emit",    true },
    { COS_V236_KIND_LARGEST_ERROR_RECOVERY, 690, 0.30f,
      "sigma",    "detected drift and rolled back",      true },
    { COS_V236_KIND_FIRST_LEGACY_ADOPTED,   780, 0.26f,
      "identity", "v233 testament accepted as heir",     true },
};

static float mean_sigma_for_domain(const cos_v236_state_t *s,
                                   const char *domain) {
    float acc = 0.0f;
    int   n   = 0;
    for (int i = 0; i < COS_V236_N_MILESTONES; ++i) {
        if (strcmp(s->milestones[i].domain, domain) == 0) {
            acc += s->milestones[i].sigma;
            ++n;
        }
    }
    return n > 0 ? acc / (float)n : 1.0f;
}

static int pick_domain_extreme(const cos_v236_state_t *s,
                               bool want_low,
                               char out[16]) {
    char  best_domain[16];
    float best_sigma = want_low ? 2.0f : -1.0f;
    best_domain[0] = '\0';

    /* Consider every distinct domain in declaration
     * order; tiebreak on alphabetical comparison. */
    for (int i = 0; i < COS_V236_N_MILESTONES; ++i) {
        const char *d = s->milestones[i].domain;
        /* skip duplicates */
        bool seen = false;
        for (int j = 0; j < i; ++j)
            if (strcmp(s->milestones[j].domain, d) == 0) { seen = true; break; }
        if (seen) continue;

        float ms = mean_sigma_for_domain(s, d);
        bool  replace = false;
        if (want_low) {
            if (ms < best_sigma - 1e-6f) replace = true;
            else if (ms <= best_sigma + 1e-6f &&
                     best_domain[0] != '\0' &&
                     strcmp(d, best_domain) < 0) replace = true;
        } else {
            if (ms > best_sigma + 1e-6f) replace = true;
            else if (ms >= best_sigma - 1e-6f &&
                     best_domain[0] != '\0' &&
                     strcmp(d, best_domain) < 0) replace = true;
        }
        if (replace || best_domain[0] == '\0') {
            best_sigma = ms;
            cpy_name(best_domain, 16, d);
        }
    }
    cpy_name(out, 16, best_domain);
    return 0;
}

void cos_v236_init(cos_v236_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x236B105ULL;
}

void cos_v236_run(cos_v236_state_t *s) {
    uint64_t prev = 0x236F1E5ULL;

    s->n_milestones = COS_V236_N_MILESTONES;
    s->first_tick = FIX[0].tick;
    s->last_tick  = FIX[COS_V236_N_MILESTONES - 1].tick;

    float total_weight    = 0.0f;
    float consistent_mass = 0.0f;

    for (int i = 0; i < COS_V236_N_MILESTONES; ++i) {
        cos_v236_milestone_t *m = &s->milestones[i];
        memset(m, 0, sizeof(*m));
        m->id       = i;
        m->kind     = FIX[i].kind;
        m->tick     = FIX[i].tick;
        m->sigma    = FIX[i].sigma;
        cpy_name(m->domain,  sizeof(m->domain),  FIX[i].domain);
        cpy_name(m->content, sizeof(m->content), FIX[i].content);
        m->consistent_with_prev = FIX[i].consistent_with_prev;

        float w = 1.0f - m->sigma;
        if (w < 0.0f) w = 0.0f;
        total_weight += w;
        if (m->consistent_with_prev) consistent_mass += w;

        struct { int id, kind, tick, cons; float sg;
                 char dom[16]; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id = m->id; rec.kind = (int)m->kind;
        rec.tick = m->tick; rec.cons = m->consistent_with_prev ? 1 : 0;
        rec.sg = m->sigma;
        cpy_name(rec.dom, sizeof(rec.dom), m->domain);
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->sigma_autobiography = total_weight > 0.0f
        ? consistent_mass / total_weight : 0.0f;

    pick_domain_extreme(s, /*want_low=*/true,  s->strongest_domain);
    pick_domain_extreme(s, /*want_low=*/false, s->weakest_domain);

    snprintf(s->narrative, sizeof(s->narrative),
        "born at tick %d, lived through %d milestones, "
        "strongest in %s, weakest in %s, sigma_auto=%.3f.",
        s->first_tick, s->n_milestones,
        s->strongest_domain, s->weakest_domain,
        s->sigma_autobiography);

    struct { int n, ft, lt; float sa;
             char sd[16], wd[16]; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.n = s->n_milestones; trec.ft = s->first_tick;
    trec.lt = s->last_tick;   trec.sa = s->sigma_autobiography;
    cpy_name(trec.sd, sizeof(trec.sd), s->strongest_domain);
    cpy_name(trec.wd, sizeof(trec.wd), s->weakest_domain);
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v236_to_json(const cos_v236_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v236\","
        "\"n_milestones\":%d,"
        "\"first_tick\":%d,\"last_tick\":%d,"
        "\"sigma_autobiography\":%.4f,"
        "\"strongest_domain\":\"%s\",\"weakest_domain\":\"%s\","
        "\"narrative\":\"%s\","
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"milestones\":[",
        s->n_milestones,
        s->first_tick, s->last_tick,
        s->sigma_autobiography,
        s->strongest_domain, s->weakest_domain,
        s->narrative,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V236_N_MILESTONES; ++i) {
        const cos_v236_milestone_t *m = &s->milestones[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"kind\":%d,\"tick\":%d,\"sigma\":%.4f,"
            "\"domain\":\"%s\",\"content\":\"%s\","
            "\"consistent_with_prev\":%s}",
            i == 0 ? "" : ",",
            m->id, (int)m->kind, m->tick, m->sigma,
            m->domain, m->content,
            m->consistent_with_prev ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int mlen = snprintf(buf + off, cap - off, "]}");
    if (mlen < 0 || off + (size_t)mlen >= cap) return 0;
    return off + (size_t)mlen;
}

int cos_v236_self_test(void) {
    cos_v236_state_t s;
    cos_v236_init(&s, 0x236B105ULL);
    cos_v236_run(&s);
    if (!s.chain_valid) return 1;

    if (s.n_milestones != COS_V236_N_MILESTONES) return 2;
    for (int i = 0; i < COS_V236_N_MILESTONES; ++i) {
        const cos_v236_milestone_t *m = &s.milestones[i];
        if (m->sigma < 0.0f || m->sigma > 1.0f) return 3;
        if (m->tick <= 0) return 4;
        if (i > 0 && m->tick <= s.milestones[i - 1].tick) return 5;
        if (!m->consistent_with_prev) return 6;
        if (m->domain[0] == '\0')  return 7;
        if (m->content[0] == '\0') return 7;
    }
    if (s.first_tick != s.milestones[0].tick) return 8;
    if (s.last_tick  != s.milestones[COS_V236_N_MILESTONES - 1].tick) return 8;

    if (!(s.sigma_autobiography > 1.0f - 1e-6f &&
          s.sigma_autobiography < 1.0f + 1e-6f)) return 9;

    if (s.strongest_domain[0] == '\0') return 10;
    if (s.weakest_domain  [0] == '\0') return 10;
    if (strcmp(s.strongest_domain, s.weakest_domain) == 0) return 11;
    if (s.narrative[0] == '\0') return 12;

    /* Determinism. */
    cos_v236_state_t t;
    cos_v236_init(&t, 0x236B105ULL);
    cos_v236_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 13;
    return 0;
}
