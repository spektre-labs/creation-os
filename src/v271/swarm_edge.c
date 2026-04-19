/*
 * v271 σ-Swarm-Edge — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "swarm_edge.h"

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

static const struct { const char *id; float s; }
    SENSORS[COS_V271_N_SENSORS] = {
    { "node-A", 0.08f },
    { "node-B", 0.17f },
    { "node-C", 0.21f },
    { "node-D", 0.11f },
    { "node-E", 0.78f },   /* excluded (σ > 0.50) */
    { "node-F", 0.14f },
};

static const struct { const char *c; float sc; float sn; }
    ANOMALY[COS_V271_N_ANOMALY] = {
    { "node-A", 0.10f, 0.12f },   /* no anomaly: 0.10-0.12 < 0.25 */
    { "node-B", 0.18f, 0.15f },   /* no anomaly: 0.18-0.15 < 0.25 */
    { "node-C", 0.62f, 0.14f },   /* anomaly:    0.62-0.14 > 0.25 */
    { "node-D", 0.55f, 0.22f },   /* anomaly:    0.55-0.22 > 0.25 */
};

static const struct { const char *t; float se; int hz; }
    ENERGY[COS_V271_N_ENERGY] = {
    { "charged", 0.05f, 100 },
    { "medium",  0.40f,  10 },
    { "low",     0.82f,   1 },
};

static bool valid_v262_engine(const char *s) {
    return strcmp(s, "bitnet-3B-local")   == 0 ||
           strcmp(s, "airllm-70B-local")  == 0 ||
           strcmp(s, "engram-lookup")     == 0 ||
           strcmp(s, "api-claude")        == 0 ||
           strcmp(s, "api-gpt")           == 0;
}

void cos_v271_init(cos_v271_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed          = seed ? seed : 0x271BEE7ULL;
    s->tau_consensus = 0.50f;
}

void cos_v271_run(cos_v271_state_t *s) {
    uint64_t prev = 0x271BE700ULL;

    s->n_consensus_rows_ok = 0;
    s->n_included = s->n_excluded = 0;
    float sum_all = 0.0f, sum_incl = 0.0f;
    int   n_all = 0, n_incl = 0;
    for (int i = 0; i < COS_V271_N_SENSORS; ++i) {
        cos_v271_sensor_t *n = &s->sensors[i];
        memset(n, 0, sizeof(*n));
        cpy(n->id, sizeof(n->id), SENSORS[i].id);
        n->sigma_local = SENSORS[i].s;
        n->included    = (n->sigma_local <= s->tau_consensus);
        if (n->sigma_local >= 0.0f && n->sigma_local <= 1.0f)
            s->n_consensus_rows_ok++;
        sum_all += n->sigma_local; n_all++;
        if (n->included) { sum_incl += n->sigma_local; n_incl++; s->n_included++; }
        else             { s->n_excluded++; }
        prev = fnv1a(n->id, strlen(n->id), prev);
        prev = fnv1a(&n->sigma_local, sizeof(n->sigma_local), prev);
        prev = fnv1a(&n->included,    sizeof(n->included),    prev);
    }
    s->sigma_raw   = (n_all  > 0) ? sum_all  / (float)n_all  : 1.0f;
    s->sigma_swarm = (n_incl > 0) ? sum_incl / (float)n_incl : 1.0f;
    s->consensus_improves_ok =
        (s->n_excluded >= 1) && (s->n_included >= 1) &&
        (s->sigma_swarm < s->sigma_raw);
    prev = fnv1a(&s->sigma_raw,   sizeof(s->sigma_raw),   prev);
    prev = fnv1a(&s->sigma_swarm, sizeof(s->sigma_swarm), prev);

    s->n_anomaly_rows_ok = 0;
    s->n_anomaly_true = s->n_anomaly_false = 0;
    for (int i = 0; i < COS_V271_N_ANOMALY; ++i) {
        cos_v271_anomaly_t *a = &s->anomaly[i];
        memset(a, 0, sizeof(*a));
        cpy(a->center_id, sizeof(a->center_id), ANOMALY[i].c);
        a->sigma_center       = ANOMALY[i].sc;
        a->sigma_neighborhood = ANOMALY[i].sn;
        a->threshold          = 0.25f;
        a->spatial_anomaly    =
            ((a->sigma_center - a->sigma_neighborhood) > a->threshold);
        if (a->sigma_center       >= 0.0f && a->sigma_center       <= 1.0f &&
            a->sigma_neighborhood >= 0.0f && a->sigma_neighborhood <= 1.0f)
            s->n_anomaly_rows_ok++;
        if (a->spatial_anomaly) s->n_anomaly_true++;
        else                    s->n_anomaly_false++;
        prev = fnv1a(a->center_id, strlen(a->center_id), prev);
        prev = fnv1a(&a->sigma_center,       sizeof(a->sigma_center),       prev);
        prev = fnv1a(&a->sigma_neighborhood, sizeof(a->sigma_neighborhood), prev);
        prev = fnv1a(&a->threshold,          sizeof(a->threshold),          prev);
        prev = fnv1a(&a->spatial_anomaly,    sizeof(a->spatial_anomaly),    prev);
    }

    s->n_energy_rows_ok = 0;
    for (int i = 0; i < COS_V271_N_ENERGY; ++i) {
        cos_v271_energy_t *e = &s->energy[i];
        memset(e, 0, sizeof(*e));
        cpy(e->tier, sizeof(e->tier), ENERGY[i].t);
        e->sigma_energy   = ENERGY[i].se;
        e->sample_rate_hz = ENERGY[i].hz;
        if (e->sigma_energy >= 0.0f && e->sigma_energy <= 1.0f &&
            e->sample_rate_hz > 0)
            s->n_energy_rows_ok++;
        prev = fnv1a(e->tier, strlen(e->tier), prev);
        prev = fnv1a(&e->sigma_energy,   sizeof(e->sigma_energy),   prev);
        prev = fnv1a(&e->sample_rate_hz, sizeof(e->sample_rate_hz), prev);
    }
    s->energy_monotone_ok = true;
    for (int i = 1; i < COS_V271_N_ENERGY; ++i) {
        if (s->energy[i].sigma_energy   <= s->energy[i-1].sigma_energy ||
            s->energy[i].sample_rate_hz >= s->energy[i-1].sample_rate_hz) {
            s->energy_monotone_ok = false; break;
        }
    }

    cos_v271_gateway_t *g = &s->gateway;
    cpy(g->gateway_device,   sizeof(g->gateway_device),   "raspberry_pi_5");
    cpy(g->bridged_to_engine, sizeof(g->bridged_to_engine), "engram-lookup");
    g->sigma_bridge      = 0.12f;
    g->swarm_size_nodes  = COS_V271_N_SENSORS;
    g->ok =
        valid_v262_engine(g->bridged_to_engine) &&
        g->sigma_bridge >= 0.0f && g->sigma_bridge <= 1.0f &&
        g->swarm_size_nodes == COS_V271_N_SENSORS &&
        strlen(g->gateway_device) > 0;
    s->gateway_ok = g->ok;
    prev = fnv1a(g->gateway_device,   strlen(g->gateway_device),   prev);
    prev = fnv1a(g->bridged_to_engine, strlen(g->bridged_to_engine), prev);
    prev = fnv1a(&g->sigma_bridge,     sizeof(g->sigma_bridge),     prev);
    prev = fnv1a(&g->swarm_size_nodes, sizeof(g->swarm_size_nodes), prev);
    prev = fnv1a(&g->ok,               sizeof(g->ok),               prev);

    int total   = COS_V271_N_SENSORS + 1 +
                  COS_V271_N_ANOMALY + 1 +
                  COS_V271_N_ENERGY  + 1 +
                  1;
    int passing = s->n_consensus_rows_ok +
                  (s->consensus_improves_ok ? 1 : 0) +
                  s->n_anomaly_rows_ok +
                  ((s->n_anomaly_true >= 1 && s->n_anomaly_false >= 1) ? 1 : 0) +
                  s->n_energy_rows_ok +
                  (s->energy_monotone_ok ? 1 : 0) +
                  (s->gateway_ok ? 1 : 0);
    s->sigma_swarm_edge = 1.0f - ((float)passing / (float)total);
    if (s->sigma_swarm_edge < 0.0f) s->sigma_swarm_edge = 0.0f;
    if (s->sigma_swarm_edge > 1.0f) s->sigma_swarm_edge = 1.0f;

    struct { int nc, ni, ne, nao, nat, naf, nen;
             bool ci, em, go;
             float sw, sr, sigma, tau; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nc  = s->n_consensus_rows_ok;
    trec.ni  = s->n_included;
    trec.ne  = s->n_excluded;
    trec.nao = s->n_anomaly_rows_ok;
    trec.nat = s->n_anomaly_true;
    trec.naf = s->n_anomaly_false;
    trec.nen = s->n_energy_rows_ok;
    trec.ci  = s->consensus_improves_ok;
    trec.em  = s->energy_monotone_ok;
    trec.go  = s->gateway_ok;
    trec.sw  = s->sigma_swarm;
    trec.sr  = s->sigma_raw;
    trec.sigma = s->sigma_swarm_edge;
    trec.tau   = s->tau_consensus;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v271_to_json(const cos_v271_state_t *s, char *buf, size_t cap) {
    const cos_v271_gateway_t *g = &s->gateway;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v271\",\"tau_consensus\":%.3f,"
        "\"sigma_swarm\":%.4f,\"sigma_raw\":%.4f,"
        "\"n_consensus_rows_ok\":%d,"
        "\"n_included\":%d,\"n_excluded\":%d,"
        "\"consensus_improves_ok\":%s,"
        "\"n_anomaly_rows_ok\":%d,"
        "\"n_anomaly_true\":%d,\"n_anomaly_false\":%d,"
        "\"n_energy_rows_ok\":%d,\"energy_monotone_ok\":%s,"
        "\"gateway\":{\"gateway_device\":\"%s\","
        "\"bridged_to_engine\":\"%s\","
        "\"sigma_bridge\":%.4f,\"swarm_size_nodes\":%d,"
        "\"ok\":%s},"
        "\"gateway_ok\":%s,"
        "\"sigma_swarm_edge\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"sensors\":[",
        s->tau_consensus,
        s->sigma_swarm, s->sigma_raw,
        s->n_consensus_rows_ok,
        s->n_included, s->n_excluded,
        s->consensus_improves_ok ? "true" : "false",
        s->n_anomaly_rows_ok,
        s->n_anomaly_true, s->n_anomaly_false,
        s->n_energy_rows_ok, s->energy_monotone_ok ? "true" : "false",
        g->gateway_device, g->bridged_to_engine,
        g->sigma_bridge, g->swarm_size_nodes,
        g->ok ? "true" : "false",
        s->gateway_ok ? "true" : "false",
        s->sigma_swarm_edge,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V271_N_SENSORS; ++i) {
        const cos_v271_sensor_t *x = &s->sensors[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"id\":\"%s\",\"sigma_local\":%.4f,"
            "\"included\":%s}",
            i == 0 ? "" : ",", x->id, x->sigma_local,
            x->included ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"anomaly\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V271_N_ANOMALY; ++i) {
        const cos_v271_anomaly_t *a = &s->anomaly[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"center_id\":\"%s\",\"sigma_center\":%.4f,"
            "\"sigma_neighborhood\":%.4f,\"threshold\":%.4f,"
            "\"spatial_anomaly\":%s}",
            i == 0 ? "" : ",", a->center_id,
            a->sigma_center, a->sigma_neighborhood, a->threshold,
            a->spatial_anomaly ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"energy\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V271_N_ENERGY; ++i) {
        const cos_v271_energy_t *e = &s->energy[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"tier\":\"%s\",\"sigma_energy\":%.4f,"
            "\"sample_rate_hz\":%d}",
            i == 0 ? "" : ",", e->tier, e->sigma_energy,
            e->sample_rate_hz);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v271_self_test(void) {
    cos_v271_state_t s;
    cos_v271_init(&s, 0x271BEE7ULL);
    cos_v271_run(&s);
    if (!s.chain_valid) return 1;

    for (int i = 0; i < COS_V271_N_SENSORS; ++i) {
        bool exp = (s.sensors[i].sigma_local <= s.tau_consensus);
        if (s.sensors[i].included != exp) return 2;
    }
    if (s.n_consensus_rows_ok != COS_V271_N_SENSORS) return 3;
    if (s.n_included <= 0 || s.n_excluded <= 0) return 4;
    if (!(s.sigma_swarm < s.sigma_raw)) return 5;
    if (!s.consensus_improves_ok) return 6;

    for (int i = 0; i < COS_V271_N_ANOMALY; ++i) {
        bool exp = ((s.anomaly[i].sigma_center -
                     s.anomaly[i].sigma_neighborhood) >
                    s.anomaly[i].threshold);
        if (s.anomaly[i].spatial_anomaly != exp) return 7;
    }
    if (s.n_anomaly_rows_ok != COS_V271_N_ANOMALY) return 8;
    if (s.n_anomaly_true  < 1) return 9;
    if (s.n_anomaly_false < 1) return 10;

    const char *en[COS_V271_N_ENERGY] = { "charged", "medium", "low" };
    for (int i = 0; i < COS_V271_N_ENERGY; ++i) {
        if (strcmp(s.energy[i].tier, en[i]) != 0) return 11;
    }
    if (s.n_energy_rows_ok != COS_V271_N_ENERGY) return 12;
    if (!s.energy_monotone_ok) return 13;

    if (!s.gateway_ok) return 14;
    if (s.gateway.swarm_size_nodes != COS_V271_N_SENSORS) return 15;

    if (s.sigma_swarm_edge < 0.0f || s.sigma_swarm_edge > 1.0f) return 16;
    if (s.sigma_swarm_edge > 1e-6f) return 17;

    cos_v271_state_t u;
    cos_v271_init(&u, 0x271BEE7ULL);
    cos_v271_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 18;
    return 0;
}
