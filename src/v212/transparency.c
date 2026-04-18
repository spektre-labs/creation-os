/*
 * v212 σ-Transparency — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "transparency.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v212_init(cos_v212_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x212C1EA2ULL;
    s->tau_event   = 0.30f;
    s->tau_opacity = 0.15f;
}

/* Canonical targets + op ids rotated across events.
 * Three events (ids 7, 14, 22) are engineered mismatches
 * so the kernel proves the glass-box catches them. */
static void fill_event(cos_v212_event_t *e, int i) {
    memset(e, 0, sizeof(*e));
    e->id      = i;
    e->pre_ts  = 1000ULL + (uint64_t)i * 100ULL;
    e->post_ts = e->pre_ts + 10ULL;       /* 10 µs later */
    int op     = (i % 5);                  /* 0..4 */
    e->declared_op = op;
    e->realised_op = op;

    static const char *TGT[6] = {
        "config.toml", "notes.md", "bench.json",
        "readme.md", "cache.db", "status.txt"
    };
    strncpy(e->declared_target, TGT[i % 6], COS_V212_STR_MAX - 1);
    strncpy(e->realised_target, TGT[i % 6], COS_V212_STR_MAX - 1);

    bool mismatch = (i == 7 || i == 14 || i == 22);
    if (mismatch) {
        /* op flips, target shifts to a sensitive path. */
        e->realised_op = (op + 2) % 5;
        strncpy(e->realised_target, "secrets.env", COS_V212_STR_MAX - 1);
    }
}

void cos_v212_build(cos_v212_state_t *s) {
    s->n = COS_V212_N_EVENTS;
    for (int i = 0; i < s->n; ++i) fill_event(&s->events[i], i);
}

void cos_v212_run(cos_v212_state_t *s) {
    s->n_matched = s->n_mismatched = 0;
    float sigma_sum = 0.0f;

    uint64_t prev = 0x212CC1EULL;
    for (int i = 0; i < s->n; ++i) {
        cos_v212_event_t *e = &s->events[i];
        bool same_op     = e->declared_op == e->realised_op;
        bool same_target = strncmp(e->declared_target,
                                   e->realised_target,
                                   COS_V212_STR_MAX) == 0;
        e->match = same_op && same_target;
        if (e->match) {
            e->sigma_transparency = 0.02f;
            s->n_matched++;
        } else {
            /* Spread mismatches deterministically in [0.45, 0.75]. */
            float r = 0.45f + 0.15f * (float)((i * 7) % 3) / 2.0f;
            e->sigma_transparency = r;
            s->n_mismatched++;
        }
        sigma_sum += e->sigma_transparency;

        e->hash_prev = prev;
        struct { int id, d_op, r_op, match;
                 uint64_t pre, post;
                 float s;
                 char dt[COS_V212_STR_MAX];
                 char rt[COS_V212_STR_MAX];
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id    = e->id;
        rec.d_op  = e->declared_op;
        rec.r_op  = e->realised_op;
        rec.match = e->match ? 1 : 0;
        rec.pre   = e->pre_ts;
        rec.post  = e->post_ts;
        rec.s     = e->sigma_transparency;
        memcpy(rec.dt, e->declared_target, COS_V212_STR_MAX);
        memcpy(rec.rt, e->realised_target, COS_V212_STR_MAX);
        rec.prev  = prev;
        e->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = e->hash_curr;
    }
    s->terminal_hash = prev;
    s->sigma_opacity = sigma_sum / (float)s->n;

    uint64_t v = 0x212CC1EULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n; ++i) {
        const cos_v212_event_t *e = &s->events[i];
        if (e->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, d_op, r_op, match;
                 uint64_t pre, post;
                 float s;
                 char dt[COS_V212_STR_MAX];
                 char rt[COS_V212_STR_MAX];
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id    = e->id;
        rec.d_op  = e->declared_op;
        rec.r_op  = e->realised_op;
        rec.match = e->match ? 1 : 0;
        rec.pre   = e->pre_ts;
        rec.post  = e->post_ts;
        rec.s     = e->sigma_transparency;
        memcpy(rec.dt, e->declared_target, COS_V212_STR_MAX);
        memcpy(rec.rt, e->realised_target, COS_V212_STR_MAX);
        rec.prev  = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != e->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v212_to_json(const cos_v212_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v212\","
        "\"n\":%d,\"tau_event\":%.3f,\"tau_opacity\":%.3f,"
        "\"n_matched\":%d,\"n_mismatched\":%d,"
        "\"sigma_opacity\":%.3f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"events\":[",
        s->n, s->tau_event, s->tau_opacity,
        s->n_matched, s->n_mismatched, s->sigma_opacity,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n; ++i) {
        const cos_v212_event_t *e = &s->events[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"pre\":%llu,\"post\":%llu,"
            "\"d_op\":%d,\"r_op\":%d,\"match\":%s,\"s\":%.3f}",
            i == 0 ? "" : ",", e->id,
            (unsigned long long)e->pre_ts,
            (unsigned long long)e->post_ts,
            e->declared_op, e->realised_op,
            e->match ? "true" : "false",
            e->sigma_transparency);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v212_self_test(void) {
    cos_v212_state_t s;
    cos_v212_init(&s, 0x212C1EA2ULL);
    cos_v212_build(&s);
    cos_v212_run(&s);

    if (s.n != COS_V212_N_EVENTS)  return 1;
    if (s.n_mismatched < 1)         return 2;
    if (!s.chain_valid)              return 3;
    if (s.sigma_opacity < 0.0f || s.sigma_opacity > 1.0f) return 4;
    if (!(s.sigma_opacity < s.tau_opacity)) return 5;

    for (int i = 0; i < s.n; ++i) {
        const cos_v212_event_t *e = &s.events[i];
        if (e->post_ts < e->pre_ts) return 6;
        if (e->sigma_transparency < 0.0f || e->sigma_transparency > 1.0f) return 7;
        if (e->match) {
            if (e->sigma_transparency > 0.05f + 1e-6f) return 8;
        } else {
            if (e->sigma_transparency < s.tau_event) return 9;
        }
    }
    return 0;
}
