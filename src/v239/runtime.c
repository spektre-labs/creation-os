/*
 * v239 σ-Compose-Runtime — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "runtime.h"

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

/* Canonical universe of kernels referenced by v0
 * fixtures.  Order in this array is *not* the
 * activation order; activation is derived topologically
 * from DEPS below. */
static const int UNIVERSE[COS_V239_N_KERNELS] = {
    29, 101, 106, 111, 115, 135, 147, 150, 112, 113, 121, 114
};

/* Dependency edges: child → parent.  A request's active
 * set must be closed under this relation (for every
 * active child, every parent is also active). */
typedef struct { int child; int parent; } cos_v239_edge_t;

static const cos_v239_edge_t DEPS[] = {
    { 150, 114 },
    { 114, 106 },
    { 115, 106 },
    { 111, 101 },
    { 147, 111 },
    { 135, 111 },
    { 112, 106 },
    { 113, 112 },
    { 121, 111 },
    { 106, 101 },
    { 101,  29 },
};
#define COS_V239_N_EDGES ((int)(sizeof(DEPS)/sizeof(DEPS[0])))

typedef struct {
    const char      *label;
    cos_v239_tier_t  tier;
    int              required[COS_V239_MAX_ACTIVE];
    int              n_required;
} cos_v239_fx_t;

/* Required *leaves* per tier — the closure fills in
 * the rest.  The over-budget fixture sets max_kernels
 * intentionally low so the σ-gate rejects it. */
static const cos_v239_fx_t FIX[COS_V239_N_REQUESTS] = {
    { "easy_fact",    COS_V239_TIER_EASY,
      { 29, 101, 106 }, 3 },
    { "medium_qa",    COS_V239_TIER_MEDIUM,
      { 29, 101, 106, 111, 115 }, 5 },
    { "hard_reason",  COS_V239_TIER_HARD,
      { 29, 101, 106, 111, 115, 150, 135, 147 }, 8 },
    { "code_task",    COS_V239_TIER_CODE,
      { 29, 101, 106, 111, 115, 150, 135, 147,
        112, 113, 121, 114 }, 12 },
    { "overbudget",   COS_V239_TIER_OVERBUDG,
      { 29, 101, 106, 111, 115, 150, 135, 147,
        112, 113, 121, 114 }, 12 },
};

/* Per-request max_kernels.  Row 4 (overbudget) is
 * deliberately set below the required-closure size. */
static const int MAX_KERNELS_PER_REQ[COS_V239_N_REQUESTS] = {
    20, 20, 20, 20, 8
};

static int is_active(const int *arr, int n, int id) {
    for (int i = 0; i < n; ++i) if (arr[i] == id) return 1;
    return 0;
}

static int parents_of(int child, int *out, int cap) {
    int k = 0;
    for (int i = 0; i < COS_V239_N_EDGES; ++i) {
        if (DEPS[i].child == child && k < cap) out[k++] = DEPS[i].parent;
    }
    return k;
}

/* Compute the transitive parent closure of `required`
 * into `active`, placing entries in a stable topological
 * order (parents before children).  Returns the final
 * count or -1 on overflow. */
static int compute_closure(const int *required, int n_required,
                           int *active, int cap) {
    int stack[COS_V239_MAX_ACTIVE * 4];
    int n_stack = 0;
    int n_active = 0;

    /* Seed with required leaves. */
    for (int i = 0; i < n_required; ++i) {
        if (n_stack >= (int)(sizeof(stack) / sizeof(stack[0])))
            return -1;
        stack[n_stack++] = required[i];
    }

    /* Emit in bottom-up order: process parents before
     * returning from a child.  We perform a simple DFS
     * that appends to `active` only after all parents
     * have been appended. */
    int visited[256];
    memset(visited, 0, sizeof(visited));

    int postorder_stack[COS_V239_MAX_ACTIVE * 4];
    int phase_stack    [COS_V239_MAX_ACTIVE * 4]; /* 0=enter, 1=leave */
    int n_ps = 0;

    for (int i = 0; i < n_stack; ++i) {
        postorder_stack[n_ps] = stack[i];
        phase_stack    [n_ps] = 0;
        n_ps++;
    }
    while (n_ps > 0) {
        int id    = postorder_stack[n_ps - 1];
        int phase = phase_stack    [n_ps - 1];
        if (id < 0 || id >= 256) return -1;

        if (phase == 0) {
            if (visited[id]) { n_ps--; continue; }
            phase_stack[n_ps - 1] = 1;
            int ps[8]; int np = parents_of(id, ps, 8);
            for (int i = 0; i < np; ++i) {
                if (!visited[ps[i]] &&
                    n_ps < (int)(sizeof(postorder_stack) /
                                 sizeof(postorder_stack[0]))) {
                    postorder_stack[n_ps] = ps[i];
                    phase_stack    [n_ps] = 0;
                    n_ps++;
                }
            }
        } else {
            n_ps--;
            if (visited[id]) continue;
            visited[id] = 1;
            if (n_active >= cap) return -1;
            active[n_active++] = id;
        }
    }
    return n_active;
}

void cos_v239_init(cos_v239_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x239BEEF0ULL;
    s->max_kernels = 20;
    (void)UNIVERSE;  /* used for documentation / audit context */
}

void cos_v239_run(cos_v239_state_t *s) {
    uint64_t prev = 0x239F1E5ULL;
    s->n_accepted = s->n_rejected = 0;

    int tick = 0;
    for (int i = 0; i < COS_V239_N_REQUESTS; ++i) {
        cos_v239_request_t *r = &s->requests[i];
        memset(r, 0, sizeof(*r));
        r->request_id = i;
        cpy_name(r->label, sizeof(r->label), FIX[i].label);
        r->tier        = FIX[i].tier;
        r->max_kernels = MAX_KERNELS_PER_REQ[i];
        r->n_required  = FIX[i].n_required;
        for (int k = 0; k < FIX[i].n_required; ++k)
            r->required[k] = FIX[i].required[k];

        int active[COS_V239_MAX_ACTIVE];
        int n = compute_closure(r->required, r->n_required,
                                active, COS_V239_MAX_ACTIVE);
        if (n < 0) n = COS_V239_MAX_ACTIVE + 1;  /* overflow → reject */

        if (n > r->max_kernels) {
            r->over_budget      = true;
            r->accepted         = false;
            r->n_active         = 0;
            r->closure_complete = false;
            r->topo_ok          = false;
            r->sigma_activation = 1.0f;
            s->n_rejected++;
        } else {
            /* Assign activation ticks in topological
             * order (compute_closure already emits
             * parents before children). */
            r->n_active = n;
            for (int k = 0; k < n; ++k) {
                r->active[k]              = active[k];
                r->activated_at_tick[k]   = ++tick;
            }
            /* Closure completeness audit. */
            bool closed = true;
            for (int k = 0; k < n; ++k) {
                int ps[8];
                int np = parents_of(active[k], ps, 8);
                for (int pi = 0; pi < np; ++pi) {
                    if (!is_active(active, n, ps[pi])) { closed = false; break; }
                }
                if (!closed) break;
            }
            /* Topological audit: every parent was
             * activated at a strictly earlier tick. */
            bool topo = true;
            for (int k = 0; k < n && topo; ++k) {
                int ps[8];
                int np = parents_of(active[k], ps, 8);
                for (int pi = 0; pi < np; ++pi) {
                    int p_tick = -1;
                    for (int q = 0; q < n; ++q) {
                        if (active[q] == ps[pi]) {
                            p_tick = r->activated_at_tick[q];
                            break;
                        }
                    }
                    if (p_tick < 0 || p_tick >= r->activated_at_tick[k]) {
                        topo = false; break;
                    }
                }
            }
            r->over_budget      = false;
            r->accepted         = closed && topo;
            r->closure_complete = closed;
            r->topo_ok          = topo;
            r->sigma_activation = (float)n / (float)r->max_kernels;
            if (r->sigma_activation < 0.0f) r->sigma_activation = 0.0f;
            if (r->sigma_activation > 1.0f) r->sigma_activation = 1.0f;
            if (r->accepted) s->n_accepted++;
            else             s->n_rejected++;
        }

        struct { int id, tier, mk, nr, na, ob, ac, cc, topo;
                 float sa; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = r->request_id;
        rec.tier = (int)r->tier;
        rec.mk   = r->max_kernels;
        rec.nr   = r->n_required;
        rec.na   = r->n_active;
        rec.ob   = r->over_budget      ? 1 : 0;
        rec.ac   = r->accepted         ? 1 : 0;
        rec.cc   = r->closure_complete ? 1 : 0;
        rec.topo = r->topo_ok          ? 1 : 0;
        rec.sa   = r->sigma_activation;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }

    struct { int mk, na, nr; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.mk = s->max_kernels; trec.na = s->n_accepted;
    trec.nr = s->n_rejected;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v239_to_json(const cos_v239_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v239\","
        "\"n_requests\":%d,\"max_kernels\":%d,"
        "\"n_accepted\":%d,\"n_rejected\":%d,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"requests\":[",
        COS_V239_N_REQUESTS, s->max_kernels,
        s->n_accepted, s->n_rejected,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V239_N_REQUESTS; ++i) {
        const cos_v239_request_t *r = &s->requests[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"request_id\":%d,\"label\":\"%s\","
            "\"tier\":%d,\"max_kernels\":%d,"
            "\"n_required\":%d,\"n_active\":%d,"
            "\"over_budget\":%s,\"accepted\":%s,"
            "\"closure_complete\":%s,\"topo_ok\":%s,"
            "\"sigma_activation\":%.4f,"
            "\"required\":[",
            i == 0 ? "" : ",",
            r->request_id, r->label, (int)r->tier,
            r->max_kernels, r->n_required, r->n_active,
            r->over_budget      ? "true" : "false",
            r->accepted         ? "true" : "false",
            r->closure_complete ? "true" : "false",
            r->topo_ok          ? "true" : "false",
            r->sigma_activation);
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
        for (int k = 0; k < r->n_required; ++k) {
            int z = snprintf(buf + off, cap - off, "%s%d",
                             k == 0 ? "" : ",", r->required[k]);
            if (z < 0 || off + (size_t)z >= cap) return 0;
            off += (size_t)z;
        }
        q = snprintf(buf + off, cap - off, "],\"active\":[");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
        for (int k = 0; k < r->n_active; ++k) {
            int z = snprintf(buf + off, cap - off,
                             "%s{\"id\":%d,\"activated_at_tick\":%d}",
                             k == 0 ? "" : ",",
                             r->active[k], r->activated_at_tick[k]);
            if (z < 0 || off + (size_t)z >= cap) return 0;
            off += (size_t)z;
        }
        q = snprintf(buf + off, cap - off, "]}");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v239_self_test(void) {
    cos_v239_state_t s;
    cos_v239_init(&s, 0x239BEEF0ULL);
    cos_v239_run(&s);
    if (!s.chain_valid) return 1;

    int n_over = 0;
    for (int i = 0; i < COS_V239_N_REQUESTS; ++i) {
        const cos_v239_request_t *r = &s.requests[i];
        if (r->n_required < 0) return 2;
        if (r->over_budget) {
            if (r->accepted) return 3;
            ++n_over;
            continue;
        }
        if (!r->accepted)         return 4;
        if (!r->closure_complete) return 5;
        if (!r->topo_ok)          return 6;
        if (r->n_active > r->max_kernels) return 7;
        if (r->sigma_activation < 0.0f || r->sigma_activation > 1.0f) return 8;
        float expected = (float)r->n_active / (float)r->max_kernels;
        float diff = r->sigma_activation - expected;
        if (diff < 0.0f) diff = -diff;
        if (diff > 1e-6f) return 9;
    }
    if (n_over != 1) return 10;
    if (s.n_rejected < 1) return 11;
    if (s.n_accepted < 4) return 12;

    /* Determinism. */
    cos_v239_state_t t;
    cos_v239_init(&t, 0x239BEEF0ULL);
    cos_v239_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 13;
    return 0;
}
