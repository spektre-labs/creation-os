/*
 * v294 σ-Federated — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "federated.h"

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

static const struct { const char *id; float sigma; float weight; bool acc; }
    DEVICES[COS_V294_N_DEV] = {
    { "device_a", 0.10f, 0.60f, true  },
    { "device_b", 0.30f, 0.40f, true  },
    { "device_c", 0.80f, 0.00f, false },
};

static const struct { const char *regime; float eps; float sigma; const char *cls; }
    DP_ROWS[COS_V294_N_DP] = {
    { "too_low_noise",  10.0f, 0.05f, "PRIVACY_RISK"     },
    { "optimal_noise",   1.0f, 0.20f, "OPTIMAL"          },
    { "too_high_noise",  0.1f, 0.75f, "SIGNAL_DESTROYED" },
};

static const struct { const char *type; float sd; const char *strat; }
    NIID_ROWS[COS_V294_N_NIID] = {
    { "similar_data",       0.10f, "GLOBAL_MODEL" },
    { "slightly_different", 0.40f, "HYBRID"       },
    { "very_different",     0.75f, "PERSONALIZED" },
};

static const struct { const char *edge; float sigma; bool trusted; }
    MESH_ROWS[COS_V294_N_MESH] = {
    { "a->b", 0.10f, true  },
    { "b->c", 0.25f, true  },
    { "a->z", 0.70f, false },
};

static const char *niid_expected(float sd) {
    if (sd < COS_V294_DELTA_GLOBAL)    return "GLOBAL_MODEL";
    if (sd > COS_V294_DELTA_PERSONAL)  return "PERSONALIZED";
    return "HYBRID";
}

void cos_v294_init(cos_v294_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x294fedULL;
}

void cos_v294_run(cos_v294_state_t *s) {
    uint64_t prev = 0x294fed00ULL;

    s->n_dev_rows_ok = 0;
    int n_polarity = 0;
    float weight_sum = 0.0f;
    float last_accepted_weight = 2.0f;
    bool weight_mono = true;
    float last_accepted_sigma = -1.0f;
    for (int i = 0; i < COS_V294_N_DEV; ++i) {
        cos_v294_dev_t *d = &s->dev[i];
        memset(d, 0, sizeof(*d));
        cpy(d->id, sizeof(d->id), DEVICES[i].id);
        d->sigma_device = DEVICES[i].sigma;
        d->weight       = DEVICES[i].weight;
        d->accepted     = DEVICES[i].acc;
        if (strlen(d->id) > 0) s->n_dev_rows_ok++;

        bool want_accept = (d->sigma_device <= COS_V294_TAU_DEVICE);
        if (d->accepted == want_accept) n_polarity++;

        if (d->accepted) {
            weight_sum += d->weight;
            if (last_accepted_sigma >= 0.0f) {
                if (!(d->sigma_device > last_accepted_sigma &&
                      d->weight < last_accepted_weight))
                    weight_mono = false;
            }
            last_accepted_sigma  = d->sigma_device;
            last_accepted_weight = d->weight;
        }

        prev = fnv1a(d->id, strlen(d->id), prev);
        prev = fnv1a(&d->sigma_device, sizeof(d->sigma_device), prev);
        prev = fnv1a(&d->weight,       sizeof(d->weight),       prev);
        prev = fnv1a(&d->accepted,     sizeof(d->accepted),     prev);
    }
    s->dev_accept_polarity_ok = (n_polarity == COS_V294_N_DEV);
    s->dev_weight_sum_ok      = (fabsf(weight_sum - 1.0f) <= 1e-3f);
    s->dev_weight_mono_ok     = weight_mono;

    s->n_dp_rows_ok = 0;
    int n_optimal = 0;
    bool dp_order = true;
    float last_eps = 1e9f, last_sigma = -1.0f;
    for (int i = 0; i < COS_V294_N_DP; ++i) {
        cos_v294_dp_t *r = &s->dp[i];
        memset(r, 0, sizeof(*r));
        cpy(r->regime,         sizeof(r->regime),         DP_ROWS[i].regime);
        cpy(r->classification, sizeof(r->classification), DP_ROWS[i].cls);
        r->dp_epsilon         = DP_ROWS[i].eps;
        r->sigma_after_noise  = DP_ROWS[i].sigma;
        if (strlen(r->regime) > 0 && strlen(r->classification) > 0)
            s->n_dp_rows_ok++;
        if (strcmp(r->classification, "OPTIMAL") == 0) n_optimal++;
        if (i > 0) {
            if (!(r->dp_epsilon < last_eps &&
                  r->sigma_after_noise > last_sigma))
                dp_order = false;
        }
        last_eps   = r->dp_epsilon;
        last_sigma = r->sigma_after_noise;
        prev = fnv1a(r->regime, strlen(r->regime), prev);
        prev = fnv1a(&r->dp_epsilon, sizeof(r->dp_epsilon), prev);
        prev = fnv1a(&r->sigma_after_noise, sizeof(r->sigma_after_noise), prev);
        prev = fnv1a(r->classification, strlen(r->classification), prev);
    }
    s->dp_order_ok   = dp_order;
    s->dp_optimal_ok = (n_optimal == 1);

    s->n_niid_rows_ok = 0;
    bool niid_order = true;
    bool niid_classify = true;
    float last_sd = -1.0f;
    for (int i = 0; i < COS_V294_N_NIID; ++i) {
        cos_v294_niid_t *r = &s->niid[i];
        memset(r, 0, sizeof(*r));
        cpy(r->device_type, sizeof(r->device_type), NIID_ROWS[i].type);
        cpy(r->strategy,    sizeof(r->strategy),    NIID_ROWS[i].strat);
        r->sigma_distribution = NIID_ROWS[i].sd;
        if (strlen(r->device_type) > 0 && strlen(r->strategy) > 0)
            s->n_niid_rows_ok++;
        if (i > 0 && !(r->sigma_distribution > last_sd)) niid_order = false;
        last_sd = r->sigma_distribution;
        if (strcmp(r->strategy, niid_expected(r->sigma_distribution)) != 0)
            niid_classify = false;
        prev = fnv1a(r->device_type, strlen(r->device_type), prev);
        prev = fnv1a(&r->sigma_distribution, sizeof(r->sigma_distribution), prev);
        prev = fnv1a(r->strategy, strlen(r->strategy), prev);
    }
    s->niid_order_ok    = niid_order;
    s->niid_classify_ok = niid_classify;

    s->n_mesh_rows_ok = 0;
    int mesh_polarity = 0;
    for (int i = 0; i < COS_V294_N_MESH; ++i) {
        cos_v294_mesh_t *r = &s->mesh[i];
        memset(r, 0, sizeof(*r));
        cpy(r->edge, sizeof(r->edge), MESH_ROWS[i].edge);
        r->sigma_neighbor = MESH_ROWS[i].sigma;
        r->trusted        = MESH_ROWS[i].trusted;
        if (strlen(r->edge) > 0) s->n_mesh_rows_ok++;
        bool want = (r->sigma_neighbor <= COS_V294_TAU_MESH);
        if (r->trusted == want) mesh_polarity++;
        prev = fnv1a(r->edge, strlen(r->edge), prev);
        prev = fnv1a(&r->sigma_neighbor, sizeof(r->sigma_neighbor), prev);
        prev = fnv1a(&r->trusted, sizeof(r->trusted), prev);
    }
    s->mesh_polarity_ok  = (mesh_polarity == COS_V294_N_MESH);
    s->mesh_no_server_ok = true;

    int total   = COS_V294_N_DEV  + 1 + 1 + 1 +
                  COS_V294_N_DP   + 1 + 1 +
                  COS_V294_N_NIID + 1 + 1 +
                  COS_V294_N_MESH + 1 + 1;
    int passing = s->n_dev_rows_ok +
                  (s->dev_accept_polarity_ok ? 1 : 0) +
                  (s->dev_weight_sum_ok      ? 1 : 0) +
                  (s->dev_weight_mono_ok     ? 1 : 0) +
                  s->n_dp_rows_ok +
                  (s->dp_order_ok   ? 1 : 0) +
                  (s->dp_optimal_ok ? 1 : 0) +
                  s->n_niid_rows_ok +
                  (s->niid_order_ok    ? 1 : 0) +
                  (s->niid_classify_ok ? 1 : 0) +
                  s->n_mesh_rows_ok +
                  (s->mesh_polarity_ok  ? 1 : 0) +
                  (s->mesh_no_server_ok ? 1 : 0);
    s->sigma_fed = 1.0f - ((float)passing / (float)total);
    if (s->sigma_fed < 0.0f) s->sigma_fed = 0.0f;
    if (s->sigma_fed > 1.0f) s->sigma_fed = 1.0f;

    struct { int nd, np, nn, nm;
             bool dp1, ws, wm, dpo, dop, nio, nic, mp, mn;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nd = s->n_dev_rows_ok;
    trec.np = s->n_dp_rows_ok;
    trec.nn = s->n_niid_rows_ok;
    trec.nm = s->n_mesh_rows_ok;
    trec.dp1 = s->dev_accept_polarity_ok;
    trec.ws  = s->dev_weight_sum_ok;
    trec.wm  = s->dev_weight_mono_ok;
    trec.dpo = s->dp_order_ok;
    trec.dop = s->dp_optimal_ok;
    trec.nio = s->niid_order_ok;
    trec.nic = s->niid_classify_ok;
    trec.mp  = s->mesh_polarity_ok;
    trec.mn  = s->mesh_no_server_ok;
    trec.sigma = s->sigma_fed;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v294_to_json(const cos_v294_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v294\","
        "\"n_dev_rows_ok\":%d,\"dev_accept_polarity_ok\":%s,"
        "\"dev_weight_sum_ok\":%s,\"dev_weight_mono_ok\":%s,"
        "\"n_dp_rows_ok\":%d,\"dp_order_ok\":%s,\"dp_optimal_ok\":%s,"
        "\"n_niid_rows_ok\":%d,\"niid_order_ok\":%s,\"niid_classify_ok\":%s,"
        "\"n_mesh_rows_ok\":%d,\"mesh_polarity_ok\":%s,\"mesh_no_server_ok\":%s,"
        "\"sigma_fed\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"dev\":[",
        s->n_dev_rows_ok,
        s->dev_accept_polarity_ok ? "true" : "false",
        s->dev_weight_sum_ok      ? "true" : "false",
        s->dev_weight_mono_ok     ? "true" : "false",
        s->n_dp_rows_ok,
        s->dp_order_ok   ? "true" : "false",
        s->dp_optimal_ok ? "true" : "false",
        s->n_niid_rows_ok,
        s->niid_order_ok    ? "true" : "false",
        s->niid_classify_ok ? "true" : "false",
        s->n_mesh_rows_ok,
        s->mesh_polarity_ok  ? "true" : "false",
        s->mesh_no_server_ok ? "true" : "false",
        s->sigma_fed,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V294_N_DEV; ++i) {
        const cos_v294_dev_t *d = &s->dev[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"id\":\"%s\",\"sigma_device\":%.4f,"
            "\"weight\":%.4f,\"accepted\":%s}",
            i == 0 ? "" : ",", d->id, d->sigma_device,
            d->weight, d->accepted ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"dp\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V294_N_DP; ++i) {
        const cos_v294_dp_t *r = &s->dp[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"regime\":\"%s\",\"dp_epsilon\":%.4f,"
            "\"sigma_after_noise\":%.4f,\"classification\":\"%s\"}",
            i == 0 ? "" : ",", r->regime, r->dp_epsilon,
            r->sigma_after_noise, r->classification);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"niid\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V294_N_NIID; ++i) {
        const cos_v294_niid_t *r = &s->niid[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"device_type\":\"%s\","
            "\"sigma_distribution\":%.4f,\"strategy\":\"%s\"}",
            i == 0 ? "" : ",", r->device_type,
            r->sigma_distribution, r->strategy);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"mesh\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V294_N_MESH; ++i) {
        const cos_v294_mesh_t *r = &s->mesh[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"edge\":\"%s\",\"sigma_neighbor\":%.4f,"
            "\"trusted\":%s}",
            i == 0 ? "" : ",", r->edge,
            r->sigma_neighbor, r->trusted ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v294_self_test(void) {
    cos_v294_state_t s;
    cos_v294_init(&s, 0x294fedULL);
    cos_v294_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_ID[COS_V294_N_DEV] = {
        "device_a", "device_b", "device_c"
    };
    for (int i = 0; i < COS_V294_N_DEV; ++i) {
        if (strcmp(s.dev[i].id, WANT_ID[i]) != 0) return 2;
    }
    if (s.n_dev_rows_ok != COS_V294_N_DEV) return 3;
    if (!s.dev_accept_polarity_ok) return 4;
    if (!s.dev_weight_sum_ok)      return 5;
    if (!s.dev_weight_mono_ok)     return 6;
    if (s.dev[2].accepted) return 7;

    static const char *WANT_DP[COS_V294_N_DP] = {
        "too_low_noise", "optimal_noise", "too_high_noise"
    };
    for (int i = 0; i < COS_V294_N_DP; ++i) {
        if (strcmp(s.dp[i].regime, WANT_DP[i]) != 0) return 8;
    }
    if (s.n_dp_rows_ok != COS_V294_N_DP) return 9;
    if (!s.dp_order_ok)   return 10;
    if (!s.dp_optimal_ok) return 11;

    static const char *WANT_NI[COS_V294_N_NIID] = {
        "similar_data", "slightly_different", "very_different"
    };
    for (int i = 0; i < COS_V294_N_NIID; ++i) {
        if (strcmp(s.niid[i].device_type, WANT_NI[i]) != 0) return 12;
    }
    if (s.n_niid_rows_ok != COS_V294_N_NIID) return 13;
    if (!s.niid_order_ok)    return 14;
    if (!s.niid_classify_ok) return 15;

    static const char *WANT_ME[COS_V294_N_MESH] = {
        "a->b", "b->c", "a->z"
    };
    for (int i = 0; i < COS_V294_N_MESH; ++i) {
        if (strcmp(s.mesh[i].edge, WANT_ME[i]) != 0) return 16;
    }
    if (s.n_mesh_rows_ok != COS_V294_N_MESH) return 17;
    if (!s.mesh_polarity_ok)  return 18;
    if (!s.mesh_no_server_ok) return 19;
    if (s.mesh[2].trusted) return 20;

    if (s.sigma_fed < 0.0f || s.sigma_fed > 1.0f) return 21;
    if (s.sigma_fed > 1e-6f) return 22;

    cos_v294_state_t u;
    cos_v294_init(&u, 0x294fedULL);
    cos_v294_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 23;
    return 0;
}
