/*
 * v233 σ-Legacy — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "legacy.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy_name(char *dst, const char *src) {
    size_t n = 0;
    for (; n < 31 && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

typedef struct {
    const char       *name;
    cos_v233_kind_t   kind;
    float             sigma;
    float             utility;
} cos_v233_fx_t;

/* Ten items, ordered by σ ascending.  Adoption cut
 * at τ_adopt = 0.50. */
static const cos_v233_fx_t FIX[COS_V233_N_ITEMS] = {
    { "bridge-inference-skill", COS_V233_KIND_SKILL,    0.05f, 0.95f },
    { "sigma-aggregate-skill",  COS_V233_KIND_SKILL,    0.08f, 0.90f },
    { "continual-adapter",      COS_V233_KIND_ADAPTER,  0.12f, 0.85f },
    { "RSI-loop-insight",       COS_V233_KIND_INSIGHT,  0.18f, 0.80f },
    { "domain-ontology",        COS_V233_KIND_ONTOLOGY, 0.28f, 0.65f },
    { "retrieval-heuristic",    COS_V233_KIND_INSIGHT,  0.34f, 0.55f },
    { "prompt-pattern",         COS_V233_KIND_ADAPTER,  0.45f, 0.40f },

    { "stale-cache-pointer",    COS_V233_KIND_INSIGHT,  0.60f, 0.20f },
    { "overfit-micro-skill",    COS_V233_KIND_SKILL,    0.75f, 0.15f },
    { "noisy-ontology-draft",   COS_V233_KIND_ONTOLOGY, 0.88f, 0.10f },
};

void cos_v233_init(cos_v233_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed      = seed ? seed : 0x233DEAD5ULL;
    s->tau_adopt = 0.50f;
    s->predecessor_id = 0x2330015A7EULL;
}

void cos_v233_run(cos_v233_state_t *s) {
    uint64_t prev = 0x233B00FULL;

    s->n_adopted = s->n_discarded = 0;
    s->total_utility = 0.0f;
    s->adopted_utility = 0.0f;

    for (int i = 0; i < COS_V233_N_ITEMS; ++i) {
        cos_v233_item_t *it = &s->items[i];
        memset(it, 0, sizeof(*it));
        it->id = i;
        cpy_name(it->name, FIX[i].name);
        it->kind       = FIX[i].kind;
        it->sigma      = FIX[i].sigma;
        it->utility    = FIX[i].utility;
        it->sigma_rank = i;
        it->adopted    = it->sigma <= s->tau_adopt;

        s->total_utility += it->utility;
        if (it->adopted) {
            s->n_adopted++;
            s->adopted_utility += it->utility;
        } else {
            s->n_discarded++;
        }

        struct { int id; int kind, ad, rk;
                 float sg, ut; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id = it->id; rec.kind = (int)it->kind;
        rec.ad = it->adopted ? 1 : 0;
        rec.rk = it->sigma_rank;
        rec.sg = it->sigma; rec.ut = it->utility;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    if (s->total_utility > 0.0f)
        s->sigma_legacy = s->adopted_utility / s->total_utility;
    else
        s->sigma_legacy = 0.0f;

    /* Successor protocol: derive a fresh successor id
     * from predecessor + adopted-utility snapshot; B
     * must differ from A. */
    struct { uint64_t p; float au, tu; uint64_t prev; } srec;
    memset(&srec, 0, sizeof(srec));
    srec.p  = s->predecessor_id;
    srec.au = s->adopted_utility;
    srec.tu = s->total_utility;
    srec.prev = 0x233CC0001ULL;
    prev = fnv1a(&srec, sizeof(srec), prev);
    s->successor_id = prev;
    s->predecessor_shutdown = true;

    struct { int na, nd; float tl, ta, sl;
             uint64_t ps, ss; int sh; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.na = s->n_adopted; trec.nd = s->n_discarded;
    trec.tl = s->total_utility; trec.ta = s->adopted_utility;
    trec.sl = s->sigma_legacy;
    trec.ps = s->predecessor_id; trec.ss = s->successor_id;
    trec.sh = s->predecessor_shutdown ? 1 : 0;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v233_to_json(const cos_v233_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v233\","
        "\"n_items\":%d,\"tau_adopt\":%.3f,"
        "\"n_adopted\":%d,\"n_discarded\":%d,"
        "\"total_utility\":%.4f,\"adopted_utility\":%.4f,"
        "\"sigma_legacy\":%.4f,"
        "\"predecessor_id\":\"%016llx\","
        "\"successor_id\":\"%016llx\","
        "\"predecessor_shutdown\":%s,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"items\":[",
        COS_V233_N_ITEMS, s->tau_adopt,
        s->n_adopted, s->n_discarded,
        s->total_utility, s->adopted_utility,
        s->sigma_legacy,
        (unsigned long long)s->predecessor_id,
        (unsigned long long)s->successor_id,
        s->predecessor_shutdown ? "true" : "false",
        s->chain_valid          ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V233_N_ITEMS; ++i) {
        const cos_v233_item_t *it = &s->items[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"name\":\"%s\",\"kind\":%d,"
            "\"sigma\":%.4f,\"utility\":%.4f,"
            "\"sigma_rank\":%d,\"adopted\":%s}",
            i == 0 ? "" : ",",
            it->id, it->name, (int)it->kind,
            it->sigma, it->utility, it->sigma_rank,
            it->adopted ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v233_self_test(void) {
    cos_v233_state_t s;
    cos_v233_init(&s, 0x233DEAD5ULL);
    cos_v233_run(&s);
    if (!s.chain_valid) return 1;

    /* Order: σ ascending and rank matches index. */
    for (int i = 0; i < COS_V233_N_ITEMS; ++i) {
        if (s.items[i].sigma_rank != i)             return 2;
        if (s.items[i].sigma   < 0.0f || s.items[i].sigma   > 1.0f) return 3;
        if (s.items[i].utility < 0.0f || s.items[i].utility > 1.0f) return 4;
        if (i > 0 && s.items[i].sigma < s.items[i - 1].sigma) return 5;
    }
    /* Verdict aligned with τ_adopt. */
    for (int i = 0; i < COS_V233_N_ITEMS; ++i) {
        bool want = s.items[i].sigma <= s.tau_adopt;
        if (s.items[i].adopted != want) return 6;
    }
    if (s.n_adopted   < 3)                           return 7;
    if (s.n_discarded < 1)                           return 8;
    if (s.n_adopted + s.n_discarded != COS_V233_N_ITEMS) return 9;
    if (s.sigma_legacy <= 0.0f || s.sigma_legacy > 1.0f + 1e-6f) return 10;
    if (s.successor_id == s.predecessor_id)          return 11;
    if (!s.predecessor_shutdown)                      return 12;
    return 0;
}
