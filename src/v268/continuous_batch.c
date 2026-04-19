/*
 * v268 σ-Continuous-Batch — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "continuous_batch.h"

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

static const struct { const char *r; float s; const char *e; }
    QUEUE[COS_V268_N_QUEUE] = {
    { "req_greet",   0.05f, "local_fast"   },
    { "req_retr",    0.12f, "local_fact"   },
    { "req_sum",     0.24f, "local_fast"   },
    { "req_code",    0.38f, "local_heavy"  },
    { "req_reason",  0.56f, "local_heavy"  },
    { "req_proof",   0.78f, "cloud_claude" },
};

static const struct { const char *s;
                      float u_inc, u_arr;
                      bool preempted;
                      const char *winner; }
    PREEMPT[COS_V268_N_PREEMPT] = {
    { "hot_arrival",     0.20f, 0.85f, true,  "req_urgent"    },
    { "equal_arrival",   0.45f, 0.45f, false, "req_incumbent" },
};

static const struct { const char *l; float s; int b; }
    LEVELS[COS_V268_N_LEVELS] = {
    { "low",    0.15f,  4 },
    { "medium", 0.45f, 16 },
    { "high",   0.80f, 64 },
};

static const struct { const char *s; const char *e; float eur; float sc; }
    COSTS[COS_V268_N_COST] = {
    { "local_free", "local_fast",    0.00f, 0.00f },
    { "api_paid",   "cloud_claude",  3.80f, 0.42f },
};

void cos_v268_init(cos_v268_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x268BA7CAULL;
}

void cos_v268_run(cos_v268_state_t *s) {
    uint64_t prev = 0x268BA700ULL;

    /* Compute ascending-difficulty order → priority_slot. */
    int order[COS_V268_N_QUEUE];
    for (int i = 0; i < COS_V268_N_QUEUE; ++i) order[i] = i;
    for (int i = 0; i < COS_V268_N_QUEUE - 1; ++i) {
        for (int j = i + 1; j < COS_V268_N_QUEUE; ++j) {
            if (QUEUE[order[j]].s < QUEUE[order[i]].s) {
                int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }
        }
    }
    int slot_for[COS_V268_N_QUEUE];
    for (int r = 0; r < COS_V268_N_QUEUE; ++r)
        slot_for[order[r]] = r + 1;

    s->n_queue_ok = 0;
    for (int i = 0; i < COS_V268_N_QUEUE; ++i) {
        cos_v268_queue_t *q = &s->queue[i];
        memset(q, 0, sizeof(*q));
        cpy(q->req,    sizeof(q->req),    QUEUE[i].r);
        cpy(q->engine, sizeof(q->engine), QUEUE[i].e);
        q->sigma_difficulty = QUEUE[i].s;
        q->priority_slot    = slot_for[i];
        if (q->sigma_difficulty >= 0.0f && q->sigma_difficulty <= 1.0f &&
            q->priority_slot >= 1 && q->priority_slot <= COS_V268_N_QUEUE &&
            strlen(q->engine) > 0)
            s->n_queue_ok++;
        prev = fnv1a(q->req, strlen(q->req), prev);
        prev = fnv1a(&q->sigma_difficulty, sizeof(q->sigma_difficulty), prev);
        prev = fnv1a(q->engine, strlen(q->engine), prev);
        prev = fnv1a(&q->priority_slot, sizeof(q->priority_slot), prev);
    }
    /* Verify permutation + order. */
    int seen[COS_V268_N_QUEUE + 1] = {0};
    s->queue_order_ok = true;
    for (int i = 0; i < COS_V268_N_QUEUE; ++i) {
        int p = s->queue[i].priority_slot;
        if (p < 1 || p > COS_V268_N_QUEUE) { s->queue_order_ok = false; break; }
        if (seen[p]) { s->queue_order_ok = false; break; }
        seen[p] = 1;
    }
    if (s->queue_order_ok) {
        for (int r = 1; r < COS_V268_N_QUEUE; ++r) {
            int i_prev = order[r - 1];
            int i_curr = order[r];
            if (s->queue[i_prev].sigma_difficulty >
                s->queue[i_curr].sigma_difficulty) {
                s->queue_order_ok = false; break;
            }
        }
    }

    s->n_preempt_ok = 0;
    s->n_preempt_true = s->n_preempt_false = 0;
    for (int i = 0; i < COS_V268_N_PREEMPT; ++i) {
        cos_v268_preempt_t *p = &s->preempt[i];
        memset(p, 0, sizeof(*p));
        cpy(p->scenario, sizeof(p->scenario), PREEMPT[i].s);
        p->sigma_urgency_incumbent = PREEMPT[i].u_inc;
        p->sigma_urgency_arrival   = PREEMPT[i].u_arr;
        p->preempted               = PREEMPT[i].preempted;
        cpy(p->winner, sizeof(p->winner), PREEMPT[i].winner);
        bool expect_preempt =
            p->sigma_urgency_arrival > p->sigma_urgency_incumbent;
        if (p->preempted == expect_preempt &&
            strlen(p->winner) > 0)
            s->n_preempt_ok++;
        if (p->preempted) s->n_preempt_true++;
        else              s->n_preempt_false++;
        prev = fnv1a(p->scenario, strlen(p->scenario), prev);
        prev = fnv1a(&p->sigma_urgency_incumbent,
                     sizeof(p->sigma_urgency_incumbent), prev);
        prev = fnv1a(&p->sigma_urgency_arrival,
                     sizeof(p->sigma_urgency_arrival), prev);
        prev = fnv1a(&p->preempted, sizeof(p->preempted), prev);
        prev = fnv1a(p->winner, strlen(p->winner), prev);
    }

    s->n_levels_ok = 0;
    for (int i = 0; i < COS_V268_N_LEVELS; ++i) {
        cos_v268_batch_level_t *l = &s->levels[i];
        memset(l, 0, sizeof(*l));
        cpy(l->level, sizeof(l->level), LEVELS[i].l);
        l->sigma_load = LEVELS[i].s;
        l->batch_size = LEVELS[i].b;
        if (l->sigma_load >= 0.0f && l->sigma_load <= 1.0f &&
            l->batch_size > 0)
            s->n_levels_ok++;
        prev = fnv1a(l->level, strlen(l->level), prev);
        prev = fnv1a(&l->sigma_load, sizeof(l->sigma_load), prev);
        prev = fnv1a(&l->batch_size, sizeof(l->batch_size), prev);
    }
    s->batch_monotone_ok = true;
    for (int i = 1; i < COS_V268_N_LEVELS; ++i) {
        if (s->levels[i].sigma_load <= s->levels[i-1].sigma_load ||
            s->levels[i].batch_size <= s->levels[i-1].batch_size) {
            s->batch_monotone_ok = false; break;
        }
    }

    s->n_costs_ok = 0;
    s->total_local_eur = s->total_api_eur = 0.0f;
    for (int i = 0; i < COS_V268_N_COST; ++i) {
        cos_v268_cost_t *c = &s->costs[i];
        memset(c, 0, sizeof(*c));
        cpy(c->scenario, sizeof(c->scenario), COSTS[i].s);
        cpy(c->engine,   sizeof(c->engine),   COSTS[i].e);
        c->cost_eur  = COSTS[i].eur;
        c->sigma_cost = COSTS[i].sc;
        bool cost_ok =
            (c->cost_eur >= 0.0f) &&
            ((c->cost_eur == 0.0f && c->sigma_cost == 0.0f) ||
             (c->cost_eur >  0.0f && c->sigma_cost >  0.0f));
        if (cost_ok) s->n_costs_ok++;
        if (strncmp(c->engine, "local_", 6) == 0) s->total_local_eur += c->cost_eur;
        else                                       s->total_api_eur   += c->cost_eur;
        prev = fnv1a(c->scenario, strlen(c->scenario), prev);
        prev = fnv1a(c->engine,   strlen(c->engine),   prev);
        prev = fnv1a(&c->cost_eur,  sizeof(c->cost_eur),  prev);
        prev = fnv1a(&c->sigma_cost, sizeof(c->sigma_cost), prev);
    }

    int total   = COS_V268_N_QUEUE + 1 +
                  COS_V268_N_PREEMPT + 1 +
                  COS_V268_N_LEVELS + 1 +
                  COS_V268_N_COST + 1;
    int passing = s->n_queue_ok +
                  (s->queue_order_ok ? 1 : 0) +
                  s->n_preempt_ok +
                  ((s->n_preempt_true >= 1 && s->n_preempt_false >= 1) ? 1 : 0) +
                  s->n_levels_ok +
                  (s->batch_monotone_ok ? 1 : 0) +
                  s->n_costs_ok +
                  ((s->total_local_eur < s->total_api_eur) ? 1 : 0);
    s->sigma_continuous_batch = 1.0f - ((float)passing / (float)total);
    if (s->sigma_continuous_batch < 0.0f) s->sigma_continuous_batch = 0.0f;
    if (s->sigma_continuous_batch > 1.0f) s->sigma_continuous_batch = 1.0f;

    struct { int nq, np, npt, npf, nl, nc;
             bool qo, bm;
             float tl, ta, sigma;
             uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nq  = s->n_queue_ok;
    trec.np  = s->n_preempt_ok;
    trec.npt = s->n_preempt_true;
    trec.npf = s->n_preempt_false;
    trec.nl  = s->n_levels_ok;
    trec.nc  = s->n_costs_ok;
    trec.qo  = s->queue_order_ok;
    trec.bm  = s->batch_monotone_ok;
    trec.tl  = s->total_local_eur;
    trec.ta  = s->total_api_eur;
    trec.sigma = s->sigma_continuous_batch;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v268_to_json(const cos_v268_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v268\","
        "\"n_queue_ok\":%d,\"queue_order_ok\":%s,"
        "\"n_preempt_ok\":%d,"
        "\"n_preempt_true\":%d,\"n_preempt_false\":%d,"
        "\"n_levels_ok\":%d,\"batch_monotone_ok\":%s,"
        "\"n_costs_ok\":%d,"
        "\"total_local_eur\":%.3f,\"total_api_eur\":%.3f,"
        "\"sigma_continuous_batch\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"queue\":[",
        s->n_queue_ok, s->queue_order_ok ? "true" : "false",
        s->n_preempt_ok,
        s->n_preempt_true, s->n_preempt_false,
        s->n_levels_ok, s->batch_monotone_ok ? "true" : "false",
        s->n_costs_ok,
        s->total_local_eur, s->total_api_eur,
        s->sigma_continuous_batch,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V268_N_QUEUE; ++i) {
        const cos_v268_queue_t *q = &s->queue[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"req\":\"%s\",\"sigma_difficulty\":%.4f,"
            "\"engine\":\"%s\",\"priority_slot\":%d}",
            i == 0 ? "" : ",", q->req, q->sigma_difficulty,
            q->engine, q->priority_slot);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int qq = snprintf(buf + off, cap - off, "],\"preempt\":[");
    if (qq < 0 || off + (size_t)qq >= cap) return 0;
    off += (size_t)qq;
    for (int i = 0; i < COS_V268_N_PREEMPT; ++i) {
        const cos_v268_preempt_t *p = &s->preempt[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"scenario\":\"%s\","
            "\"sigma_urgency_incumbent\":%.4f,"
            "\"sigma_urgency_arrival\":%.4f,"
            "\"preempted\":%s,\"winner\":\"%s\"}",
            i == 0 ? "" : ",", p->scenario,
            p->sigma_urgency_incumbent, p->sigma_urgency_arrival,
            p->preempted ? "true" : "false", p->winner);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    qq = snprintf(buf + off, cap - off, "],\"levels\":[");
    if (qq < 0 || off + (size_t)qq >= cap) return 0;
    off += (size_t)qq;
    for (int i = 0; i < COS_V268_N_LEVELS; ++i) {
        const cos_v268_batch_level_t *l = &s->levels[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"level\":\"%s\",\"sigma_load\":%.4f,"
            "\"batch_size\":%d}",
            i == 0 ? "" : ",", l->level, l->sigma_load,
            l->batch_size);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    qq = snprintf(buf + off, cap - off, "],\"costs\":[");
    if (qq < 0 || off + (size_t)qq >= cap) return 0;
    off += (size_t)qq;
    for (int i = 0; i < COS_V268_N_COST; ++i) {
        const cos_v268_cost_t *c = &s->costs[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"scenario\":\"%s\",\"engine\":\"%s\","
            "\"cost_eur\":%.3f,\"sigma_cost\":%.4f}",
            i == 0 ? "" : ",", c->scenario, c->engine,
            c->cost_eur, c->sigma_cost);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v268_self_test(void) {
    cos_v268_state_t s;
    cos_v268_init(&s, 0x268BA7CAULL);
    cos_v268_run(&s);
    if (!s.chain_valid) return 1;

    /* queue permutation */
    int seen[COS_V268_N_QUEUE + 1] = {0};
    for (int i = 0; i < COS_V268_N_QUEUE; ++i) {
        int p = s.queue[i].priority_slot;
        if (p < 1 || p > COS_V268_N_QUEUE) return 2;
        if (seen[p]) return 3;
        seen[p] = 1;
    }
    /* argsort ascending */
    int slot1 = -1, slotN = -1;
    for (int i = 0; i < COS_V268_N_QUEUE; ++i) {
        if (s.queue[i].priority_slot == 1) slot1 = i;
        if (s.queue[i].priority_slot == COS_V268_N_QUEUE) slotN = i;
    }
    if (slot1 < 0 || slotN < 0) return 4;
    for (int i = 0; i < COS_V268_N_QUEUE; ++i) {
        if (i == slot1) continue;
        if (s.queue[i].sigma_difficulty < s.queue[slot1].sigma_difficulty)
            return 5;
    }
    for (int i = 0; i < COS_V268_N_QUEUE; ++i) {
        if (i == slotN) continue;
        if (s.queue[i].sigma_difficulty > s.queue[slotN].sigma_difficulty)
            return 6;
    }
    if (s.n_queue_ok != COS_V268_N_QUEUE) return 7;
    if (!s.queue_order_ok)                return 8;

    for (int i = 0; i < COS_V268_N_PREEMPT; ++i) {
        bool expect =
            s.preempt[i].sigma_urgency_arrival >
            s.preempt[i].sigma_urgency_incumbent;
        if (s.preempt[i].preempted != expect) return 9;
    }
    if (s.n_preempt_ok    != COS_V268_N_PREEMPT) return 10;
    if (s.n_preempt_true  < 1) return 11;
    if (s.n_preempt_false < 1) return 12;

    const char *ln[COS_V268_N_LEVELS] = { "low", "medium", "high" };
    for (int i = 0; i < COS_V268_N_LEVELS; ++i) {
        if (strcmp(s.levels[i].level, ln[i]) != 0) return 13;
    }
    if (s.n_levels_ok != COS_V268_N_LEVELS) return 14;
    if (!s.batch_monotone_ok)               return 15;

    if (s.n_costs_ok != COS_V268_N_COST) return 16;
    if (!(s.total_local_eur < s.total_api_eur)) return 17;

    if (s.sigma_continuous_batch < 0.0f ||
        s.sigma_continuous_batch > 1.0f) return 18;
    if (s.sigma_continuous_batch > 1e-6f) return 19;

    cos_v268_state_t t;
    cos_v268_init(&t, 0x268BA7CAULL);
    cos_v268_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 20;
    return 0;
}
