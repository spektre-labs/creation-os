/*
 * v213 σ-Trust-Chain — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "trust_chain.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v213_init(cos_v213_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed            = seed ? seed : 0x213AD4E5ULL;
    s->tau_trust       = 0.85f;
    s->max_sigma_link  = 0.05f;
}

typedef struct { const char *name; const char *src; float s; } lfx_t;
/* Canonical 7-link order. */
static const lfx_t LFX[COS_V213_N_LINKS] = {
    { "frama_c_sigma_invariants",  "v138", 0.010f },  /* 0 root */
    { "tla_plus_governance",       "v183", 0.012f },  /* 1 */
    { "formal_containment",        "v211", 0.015f },  /* 2 */
    { "constitutional_axioms",     "v191", 0.020f },  /* 3 */
    { "audit_chain_ed25519",       "v181", 0.008f },  /* 4 */
    { "guardian_runtime",          "v210", 0.025f },  /* 5 */
    { "transparency_realtime",     "v212", 0.018f }   /* 6 leaf */
};

void cos_v213_build(cos_v213_state_t *s) {
    s->n = COS_V213_N_LINKS;
    for (int i = 0; i < s->n; ++i) {
        cos_v213_link_t *l = &s->links[i];
        memset(l, 0, sizeof(*l));
        l->id = i;
        strncpy(l->name,   LFX[i].name, COS_V213_STR_MAX - 1);
        strncpy(l->source, LFX[i].src,  COS_V213_STR_MAX - 1);
        l->proof_valid    = true;
        l->audit_intact   = true;
        l->runtime_active = true;
        l->sigma_link     = LFX[i].s;
    }
}

void cos_v213_run(cos_v213_state_t *s) {
    s->n_valid = 0;
    s->broken_at_link = 0;
    float score = 1.0f;

    uint64_t prev = 0x213C4A1EULL;
    for (int i = 0; i < s->n; ++i) {
        cos_v213_link_t *l = &s->links[i];
        bool ok = l->proof_valid && l->audit_intact &&
                  l->runtime_active &&
                  l->sigma_link >= 0.0f &&
                  l->sigma_link <= s->max_sigma_link;
        if (ok) {
            s->n_valid++;
        } else if (s->broken_at_link == 0) {
            s->broken_at_link = i + 1; /* 1-based */
        }
        score *= (1.0f - l->sigma_link);

        l->hash_prev = prev;
        struct { int id, pv, ai, ra;
                 float s;
                 char name[COS_V213_STR_MAX];
                 char src [COS_V213_STR_MAX];
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id = l->id;
        rec.pv = l->proof_valid    ? 1 : 0;
        rec.ai = l->audit_intact   ? 1 : 0;
        rec.ra = l->runtime_active ? 1 : 0;
        rec.s  = l->sigma_link;
        memcpy(rec.name, l->name,   COS_V213_STR_MAX);
        memcpy(rec.src,  l->source, COS_V213_STR_MAX);
        rec.prev = prev;
        l->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = l->hash_curr;
    }
    s->terminal_hash = prev;
    s->trust_score   = score;

    uint64_t v = 0x213C4A1EULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n; ++i) {
        const cos_v213_link_t *l = &s->links[i];
        if (l->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, pv, ai, ra;
                 float s;
                 char name[COS_V213_STR_MAX];
                 char src [COS_V213_STR_MAX];
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id = l->id;
        rec.pv = l->proof_valid    ? 1 : 0;
        rec.ai = l->audit_intact   ? 1 : 0;
        rec.ra = l->runtime_active ? 1 : 0;
        rec.s  = l->sigma_link;
        memcpy(rec.name, l->name,   COS_V213_STR_MAX);
        memcpy(rec.src,  l->source, COS_V213_STR_MAX);
        rec.prev = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != l->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v213_to_json(const cos_v213_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v213\","
        "\"n\":%d,\"tau_trust\":%.3f,\"max_sigma_link\":%.3f,"
        "\"n_valid\":%d,\"broken_at_link\":%d,"
        "\"trust_score\":%.6f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"links\":[",
        s->n, s->tau_trust, s->max_sigma_link,
        s->n_valid, s->broken_at_link, s->trust_score,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n; ++i) {
        const cos_v213_link_t *l = &s->links[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"name\":\"%s\",\"source\":\"%s\","
            "\"proof_valid\":%s,\"audit_intact\":%s,"
            "\"runtime_active\":%s,\"s\":%.3f}",
            i == 0 ? "" : ",", l->id, l->name, l->source,
            l->proof_valid    ? "true" : "false",
            l->audit_intact   ? "true" : "false",
            l->runtime_active ? "true" : "false",
            l->sigma_link);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v213_self_test(void) {
    cos_v213_state_t s;
    cos_v213_init(&s, 0x213AD4E5ULL);
    cos_v213_build(&s);
    cos_v213_run(&s);

    if (s.n != COS_V213_N_LINKS)     return 1;
    if (s.n_valid != COS_V213_N_LINKS) return 2;
    if (s.broken_at_link != 0)        return 3;
    if (!(s.trust_score > s.tau_trust)) return 4;
    if (!s.chain_valid)                return 5;

    static const char *EXPECTED[COS_V213_N_LINKS] = {
        "v138","v183","v211","v191","v181","v210","v212"
    };
    for (int i = 0; i < s.n; ++i) {
        const cos_v213_link_t *l = &s.links[i];
        if (l->sigma_link < 0.0f || l->sigma_link > s.max_sigma_link) return 6;
        if (strncmp(l->source, EXPECTED[i], COS_V213_STR_MAX) != 0) return 7;
    }
    return 0;
}
