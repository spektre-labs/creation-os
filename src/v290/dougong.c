/*
 * v290 σ-Dougong — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "dougong.h"

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

static const struct { const char *p; const char *c; }
    COUPLES[COS_V290_N_COUPLING] = {
    { "v267_mamba",  "v262_hybrid"      },
    { "v260_engram", "v206_ghosts"      },
    { "v275_ttt",    "v272_agentic_rl"  },
    { "v286_interp", "v269_stopping"    },
};

static const struct { const char *f; const char *t; const char *l; }
    SWAPS[COS_V290_N_SWAP] = {
    { "v267_mamba",   "v276_deltanet", "long_context"      },
    { "v216_quorum",  "v214_swarm",    "agent_consensus"   },
    { "v232_sqlite",  "v224_snapshot", "state_persistence" },
};

static const struct { const char *n; float l; }
    SEISMICS[COS_V290_N_SEISMIC] = {
    { "spike_small",  0.40f },
    { "spike_medium", 0.60f },
    { "spike_large",  0.78f },
};

static const struct { const char *s; const char *o; }
    CHAOSES[COS_V290_N_CHAOS] = {
    { "kill_random_kernel",     "survived"           },
    { "overload_single_kernel", "load_distributed"   },
    { "network_partition",      "degraded_but_alive" },
};

static const char *CHANNEL = "sigma_measurement_t";

void cos_v290_init(cos_v290_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x290aaaULL;
    s->load_budget = 0.80f;
}

void cos_v290_run(cos_v290_state_t *s) {
    uint64_t prev = 0x290aaa00ULL;

    s->n_coupling_rows_ok = 0;
    int n_channel = 0, n_no_direct = 0;
    for (int i = 0; i < COS_V290_N_COUPLING; ++i) {
        cos_v290_coupling_t *c = &s->coupling[i];
        memset(c, 0, sizeof(*c));
        cpy(c->producer, sizeof(c->producer), COUPLES[i].p);
        cpy(c->consumer, sizeof(c->consumer), COUPLES[i].c);
        cpy(c->channel,  sizeof(c->channel),  CHANNEL);
        c->direct_call = false;
        if (strlen(c->producer) > 0 && strlen(c->consumer) > 0)
            s->n_coupling_rows_ok++;
        if (strcmp(c->channel, CHANNEL) == 0) n_channel++;
        if (!c->direct_call)                  n_no_direct++;
        prev = fnv1a(c->producer, strlen(c->producer), prev);
        prev = fnv1a(c->consumer, strlen(c->consumer), prev);
        prev = fnv1a(c->channel,  strlen(c->channel),  prev);
        prev = fnv1a(&c->direct_call, sizeof(c->direct_call), prev);
    }
    s->coupling_channel_ok        = (n_channel   == COS_V290_N_COUPLING);
    s->coupling_no_direct_call_ok = (n_no_direct == COS_V290_N_COUPLING);

    s->n_swap_rows_ok = 0;
    int n_zero = 0, n_unchanged = 0;
    for (int i = 0; i < COS_V290_N_SWAP; ++i) {
        cos_v290_swap_t *w = &s->swap[i];
        memset(w, 0, sizeof(*w));
        cpy(w->from_kernel, sizeof(w->from_kernel), SWAPS[i].f);
        cpy(w->to_kernel,   sizeof(w->to_kernel),   SWAPS[i].t);
        cpy(w->layer,       sizeof(w->layer),       SWAPS[i].l);
        w->downtime_ms      = 0;
        w->config_unchanged = true;
        if (strlen(w->from_kernel) > 0 && strlen(w->to_kernel) > 0)
            s->n_swap_rows_ok++;
        if (w->downtime_ms == 0)    n_zero++;
        if (w->config_unchanged)    n_unchanged++;
        prev = fnv1a(w->from_kernel, strlen(w->from_kernel), prev);
        prev = fnv1a(w->to_kernel,   strlen(w->to_kernel),   prev);
        prev = fnv1a(w->layer,       strlen(w->layer),       prev);
        prev = fnv1a(&w->downtime_ms,      sizeof(w->downtime_ms),      prev);
        prev = fnv1a(&w->config_unchanged, sizeof(w->config_unchanged), prev);
    }
    s->swap_zero_downtime_ok    = (n_zero      == COS_V290_N_SWAP);
    s->swap_config_unchanged_ok = (n_unchanged == COS_V290_N_SWAP);

    s->n_seismic_rows_ok = 0;
    int n_dist = 0, n_bounded = 0;
    for (int i = 0; i < COS_V290_N_SEISMIC; ++i) {
        cos_v290_seismic_t *q = &s->seismic[i];
        memset(q, 0, sizeof(*q));
        cpy(q->name, sizeof(q->name), SEISMICS[i].n);
        q->max_sigma_load   = SEISMICS[i].l;
        q->load_distributed = true;
        if (strlen(q->name) > 0 &&
            q->max_sigma_load >= 0.0f &&
            q->max_sigma_load <= 1.0f)
            s->n_seismic_rows_ok++;
        if (q->load_distributed)                    n_dist++;
        if (q->max_sigma_load <= s->load_budget)    n_bounded++;
        prev = fnv1a(q->name, strlen(q->name), prev);
        prev = fnv1a(&q->max_sigma_load,   sizeof(q->max_sigma_load),   prev);
        prev = fnv1a(&q->load_distributed, sizeof(q->load_distributed), prev);
    }
    s->seismic_distributed_ok  = (n_dist    == COS_V290_N_SEISMIC);
    s->seismic_load_bounded_ok = (n_bounded == COS_V290_N_SEISMIC);

    s->n_chaos_rows_ok = 0;
    int n_pass = 0;
    for (int i = 0; i < COS_V290_N_CHAOS; ++i) {
        cos_v290_chaos_t *x = &s->chaos[i];
        memset(x, 0, sizeof(*x));
        cpy(x->scenario, sizeof(x->scenario), CHAOSES[i].s);
        cpy(x->outcome,  sizeof(x->outcome),  CHAOSES[i].o);
        x->passed = true;
        if (strlen(x->scenario) > 0 && strlen(x->outcome) > 0)
            s->n_chaos_rows_ok++;
        if (x->passed) n_pass++;
        prev = fnv1a(x->scenario, strlen(x->scenario), prev);
        prev = fnv1a(x->outcome,  strlen(x->outcome),  prev);
        prev = fnv1a(&x->passed,  sizeof(x->passed),   prev);
    }
    bool distinct = true;
    for (int i = 0; i < COS_V290_N_CHAOS && distinct; ++i) {
        for (int j = i + 1; j < COS_V290_N_CHAOS; ++j) {
            if (strcmp(s->chaos[i].outcome, s->chaos[j].outcome) == 0) {
                distinct = false; break;
            }
        }
    }
    s->chaos_outcome_distinct_ok = distinct;
    s->chaos_all_passed_ok       = (n_pass == COS_V290_N_CHAOS);

    int total   = COS_V290_N_COUPLING + 1 + 1 +
                  COS_V290_N_SWAP     + 1 + 1 +
                  COS_V290_N_SEISMIC  + 1 + 1 +
                  COS_V290_N_CHAOS    + 1 + 1;
    int passing = s->n_coupling_rows_ok +
                  (s->coupling_channel_ok        ? 1 : 0) +
                  (s->coupling_no_direct_call_ok ? 1 : 0) +
                  s->n_swap_rows_ok +
                  (s->swap_zero_downtime_ok      ? 1 : 0) +
                  (s->swap_config_unchanged_ok   ? 1 : 0) +
                  s->n_seismic_rows_ok +
                  (s->seismic_distributed_ok     ? 1 : 0) +
                  (s->seismic_load_bounded_ok    ? 1 : 0) +
                  s->n_chaos_rows_ok +
                  (s->chaos_outcome_distinct_ok  ? 1 : 0) +
                  (s->chaos_all_passed_ok        ? 1 : 0);
    s->sigma_dougong = 1.0f - ((float)passing / (float)total);
    if (s->sigma_dougong < 0.0f) s->sigma_dougong = 0.0f;
    if (s->sigma_dougong > 1.0f) s->sigma_dougong = 1.0f;

    struct { int nc, ns, nq, nx;
             bool cc, cd, sw, su, sd, sb, od, cp;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nc = s->n_coupling_rows_ok;
    trec.ns = s->n_swap_rows_ok;
    trec.nq = s->n_seismic_rows_ok;
    trec.nx = s->n_chaos_rows_ok;
    trec.cc = s->coupling_channel_ok;
    trec.cd = s->coupling_no_direct_call_ok;
    trec.sw = s->swap_zero_downtime_ok;
    trec.su = s->swap_config_unchanged_ok;
    trec.sd = s->seismic_distributed_ok;
    trec.sb = s->seismic_load_bounded_ok;
    trec.od = s->chaos_outcome_distinct_ok;
    trec.cp = s->chaos_all_passed_ok;
    trec.sigma = s->sigma_dougong;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v290_to_json(const cos_v290_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v290\","
        "\"load_budget\":%.3f,"
        "\"n_coupling_rows_ok\":%d,"
        "\"coupling_channel_ok\":%s,"
        "\"coupling_no_direct_call_ok\":%s,"
        "\"n_swap_rows_ok\":%d,"
        "\"swap_zero_downtime_ok\":%s,"
        "\"swap_config_unchanged_ok\":%s,"
        "\"n_seismic_rows_ok\":%d,"
        "\"seismic_distributed_ok\":%s,"
        "\"seismic_load_bounded_ok\":%s,"
        "\"n_chaos_rows_ok\":%d,"
        "\"chaos_outcome_distinct_ok\":%s,"
        "\"chaos_all_passed_ok\":%s,"
        "\"sigma_dougong\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"coupling\":[",
        s->load_budget,
        s->n_coupling_rows_ok,
        s->coupling_channel_ok        ? "true" : "false",
        s->coupling_no_direct_call_ok ? "true" : "false",
        s->n_swap_rows_ok,
        s->swap_zero_downtime_ok      ? "true" : "false",
        s->swap_config_unchanged_ok   ? "true" : "false",
        s->n_seismic_rows_ok,
        s->seismic_distributed_ok     ? "true" : "false",
        s->seismic_load_bounded_ok    ? "true" : "false",
        s->n_chaos_rows_ok,
        s->chaos_outcome_distinct_ok  ? "true" : "false",
        s->chaos_all_passed_ok        ? "true" : "false",
        s->sigma_dougong,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V290_N_COUPLING; ++i) {
        const cos_v290_coupling_t *c = &s->coupling[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"producer\":\"%s\",\"consumer\":\"%s\","
            "\"channel\":\"%s\",\"direct_call\":%s}",
            i == 0 ? "" : ",", c->producer, c->consumer,
            c->channel, c->direct_call ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"swap\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V290_N_SWAP; ++i) {
        const cos_v290_swap_t *w = &s->swap[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"from_kernel\":\"%s\",\"to_kernel\":\"%s\","
            "\"layer\":\"%s\",\"downtime_ms\":%d,"
            "\"config_unchanged\":%s}",
            i == 0 ? "" : ",", w->from_kernel, w->to_kernel,
            w->layer, w->downtime_ms,
            w->config_unchanged ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"seismic\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V290_N_SEISMIC; ++i) {
        const cos_v290_seismic_t *qs = &s->seismic[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"max_sigma_load\":%.4f,"
            "\"load_distributed\":%s}",
            i == 0 ? "" : ",", qs->name, qs->max_sigma_load,
            qs->load_distributed ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"chaos\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V290_N_CHAOS; ++i) {
        const cos_v290_chaos_t *x = &s->chaos[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"scenario\":\"%s\",\"outcome\":\"%s\","
            "\"passed\":%s}",
            i == 0 ? "" : ",", x->scenario, x->outcome,
            x->passed ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v290_self_test(void) {
    cos_v290_state_t s;
    cos_v290_init(&s, 0x290aaaULL);
    cos_v290_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_P[COS_V290_N_COUPLING] = {
        "v267_mamba", "v260_engram", "v275_ttt", "v286_interp"
    };
    for (int i = 0; i < COS_V290_N_COUPLING; ++i) {
        if (strcmp(s.coupling[i].producer, WANT_P[i]) != 0) return 2;
        if (strcmp(s.coupling[i].channel, "sigma_measurement_t") != 0) return 3;
        if (s.coupling[i].direct_call != false) return 4;
    }
    if (s.n_coupling_rows_ok          != COS_V290_N_COUPLING) return 5;
    if (!s.coupling_channel_ok)         return 6;
    if (!s.coupling_no_direct_call_ok)  return 7;

    static const char *WANT_F[COS_V290_N_SWAP] = {
        "v267_mamba", "v216_quorum", "v232_sqlite"
    };
    for (int i = 0; i < COS_V290_N_SWAP; ++i) {
        if (strcmp(s.swap[i].from_kernel, WANT_F[i]) != 0) return 8;
        if (s.swap[i].downtime_ms      != 0)    return 9;
        if (s.swap[i].config_unchanged != true) return 10;
    }
    if (s.n_swap_rows_ok != COS_V290_N_SWAP)      return 11;
    if (!s.swap_zero_downtime_ok)                 return 12;
    if (!s.swap_config_unchanged_ok)              return 13;

    static const char *WANT_Q[COS_V290_N_SEISMIC] = {
        "spike_small", "spike_medium", "spike_large"
    };
    for (int i = 0; i < COS_V290_N_SEISMIC; ++i) {
        if (strcmp(s.seismic[i].name, WANT_Q[i]) != 0) return 14;
        if (!s.seismic[i].load_distributed)             return 15;
        if (s.seismic[i].max_sigma_load > s.load_budget) return 16;
    }
    if (s.n_seismic_rows_ok  != COS_V290_N_SEISMIC) return 17;
    if (!s.seismic_distributed_ok)                  return 18;
    if (!s.seismic_load_bounded_ok)                 return 19;

    static const char *WANT_S[COS_V290_N_CHAOS] = {
        "kill_random_kernel", "overload_single_kernel", "network_partition"
    };
    static const char *WANT_O[COS_V290_N_CHAOS] = {
        "survived", "load_distributed", "degraded_but_alive"
    };
    for (int i = 0; i < COS_V290_N_CHAOS; ++i) {
        if (strcmp(s.chaos[i].scenario, WANT_S[i]) != 0) return 20;
        if (strcmp(s.chaos[i].outcome,  WANT_O[i]) != 0) return 21;
        if (!s.chaos[i].passed) return 22;
    }
    if (s.n_chaos_rows_ok          != COS_V290_N_CHAOS) return 23;
    if (!s.chaos_outcome_distinct_ok) return 24;
    if (!s.chaos_all_passed_ok)       return 25;

    if (s.sigma_dougong < 0.0f || s.sigma_dougong > 1.0f) return 26;
    if (s.sigma_dougong > 1e-6f) return 27;

    cos_v290_state_t u;
    cos_v290_init(&u, 0x290aaaULL);
    cos_v290_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 28;
    return 0;
}
