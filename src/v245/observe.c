/*
 * v245 σ-Observe — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "observe.h"

#include <ctype.h>
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

/* Prometheus identifier rule:
 *   first char  : letter or '_'
 *   later chars : alphanumeric, '_', or ':'
 *   at least one char */
static bool prom_name_valid(const char *s) {
    if (!s || !*s) return false;
    unsigned char c = (unsigned char)s[0];
    if (!(isalpha(c) || c == '_')) return false;
    for (const char *p = s + 1; *p; ++p) {
        c = (unsigned char)*p;
        if (!(isalnum(c) || c == '_' || c == ':')) return false;
    }
    return true;
}

typedef struct { const char *name; cos_v245_metric_type_t t; const char *help; } cos_v245_fxm_t;

static const cos_v245_fxm_t METRICS[COS_V245_N_METRICS] = {
    { "cos_sigma_product",    COS_V245_TYPE_GAUGE,     "per-request sigma_product" },
    { "cos_k_eff",            COS_V245_TYPE_GAUGE,     "system-wide K_eff"         },
    { "cos_requests_total",   COS_V245_TYPE_COUNTER,   "total requests served"     },
    { "cos_latency_seconds",  COS_V245_TYPE_HISTOGRAM, "request latency seconds"   },
    { "cos_tokens_per_second",COS_V245_TYPE_GAUGE,     "tokens per second"         },
    { "cos_kernels_active",   COS_V245_TYPE_GAUGE,     "kernels currently active"  },
    { "cos_abstain_rate",     COS_V245_TYPE_GAUGE,     "abstain rate in [0,1]"     },
};

static const char *LOG_FIELDS[COS_V245_N_LOG_FIELDS] = {
    "id", "timestamp", "level", "sigma_product",
    "latency_ms", "kernels_used", "pipeline_type", "presence_state"
};

static const char *LOG_LEVELS[COS_V245_N_LOG_LEVELS] = {
    "DEBUG", "INFO", "WARN", "ERROR"
};

static const struct { const char *name; float sigma; } SPANS[COS_V245_N_TRACE_SPANS] = {
    { "reason",  0.18f },
    { "recall",  0.15f },
    { "verify",  0.21f },
    { "reflect", 0.23f },
    { "emit",    0.12f },
};

static const struct { const char *id; const char *cond; } ALERTS[COS_V245_N_ALERTS] = {
    { "A1", "sigma_product > tau_alert (0.60)" },
    { "A2", "k_eff < k_crit (0.70)"            },
    { "A3", "abstain_rate > 0.30"              },
    { "A4", "guardian_anomaly == true (v210)"  },
};

static const char *CHANNELS[COS_V245_N_CHANNELS] = {
    "webhook", "email", "slack"
};

void cos_v245_init(cos_v245_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed      = seed ? seed : 0x2450B5E7ULL;
    s->tau_alert = 0.60f;
    s->k_crit    = 0.70f;
    cpy(s->scrape_endpoint, sizeof(s->scrape_endpoint), "GET /metrics");
}

void cos_v245_run(cos_v245_state_t *s) {
    uint64_t prev = 0x2450B5E5ULL;

    s->n_metric_names_valid = 0;
    for (int i = 0; i < COS_V245_N_METRICS; ++i) {
        cos_v245_metric_t *m = &s->metrics[i];
        memset(m, 0, sizeof(*m));
        cpy(m->name, sizeof(m->name), METRICS[i].name);
        cpy(m->help, sizeof(m->help), METRICS[i].help);
        m->type       = METRICS[i].t;
        m->name_valid = prom_name_valid(m->name);
        if (m->name_valid) s->n_metric_names_valid++;
        prev = fnv1a(m->name, strlen(m->name), prev);
        prev = fnv1a(&m->type, sizeof(m->type), prev);
    }

    for (int i = 0; i < COS_V245_N_LOG_FIELDS; ++i) {
        cpy(s->log_fields[i], sizeof(s->log_fields[i]), LOG_FIELDS[i]);
        prev = fnv1a(s->log_fields[i], strlen(s->log_fields[i]), prev);
    }
    for (int i = 0; i < COS_V245_N_LOG_LEVELS; ++i) {
        cpy(s->log_levels[i], sizeof(s->log_levels[i]), LOG_LEVELS[i]);
        prev = fnv1a(s->log_levels[i], strlen(s->log_levels[i]), prev);
    }

    float max_sigma = 0.0f;
    int tick = 0;
    for (int i = 0; i < COS_V245_N_TRACE_SPANS; ++i) {
        cos_v245_span_t *sp = &s->spans[i];
        memset(sp, 0, sizeof(*sp));
        cpy(sp->name, sizeof(sp->name), SPANS[i].name);
        sp->sigma = SPANS[i].sigma;
        sp->tick  = ++tick;
        if (sp->sigma > max_sigma) max_sigma = sp->sigma;
        prev = fnv1a(sp->name, strlen(sp->name), prev);
        prev = fnv1a(&sp->sigma, sizeof(sp->sigma), prev);
    }
    s->n_spans                 = COS_V245_N_TRACE_SPANS;
    s->sigma_trace             = max_sigma;
    s->sigma_product_request   = 0.23f;
    s->total_latency_ms        = 340;

    for (int i = 0; i < COS_V245_N_ALERTS; ++i) {
        cos_v245_alert_t *a = &s->alerts[i];
        memset(a, 0, sizeof(*a));
        cpy(a->id,        sizeof(a->id),        ALERTS[i].id);
        cpy(a->condition, sizeof(a->condition), ALERTS[i].cond);
        a->n_channels = COS_V245_N_CHANNELS;
        for (int k = 0; k < COS_V245_N_CHANNELS; ++k)
            cpy(a->channels[k], sizeof(a->channels[k]), CHANNELS[k]);
        prev = fnv1a(a->id,        strlen(a->id),        prev);
        prev = fnv1a(a->condition, strlen(a->condition), prev);
    }

    s->sigma_observe = 1.0f - ((float)s->n_metric_names_valid /
                               (float)COS_V245_N_METRICS);
    if (s->sigma_observe < 0.0f) s->sigma_observe = 0.0f;
    if (s->sigma_observe > 1.0f) s->sigma_observe = 1.0f;

    struct { int nm, nv, ns; float st, so, ta, kc; int lat;
             uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nm  = COS_V245_N_METRICS;
    trec.nv  = s->n_metric_names_valid;
    trec.ns  = s->n_spans;
    trec.st  = s->sigma_trace;
    trec.so  = s->sigma_observe;
    trec.ta  = s->tau_alert;
    trec.kc  = s->k_crit;
    trec.lat = s->total_latency_ms;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *type_name(cos_v245_metric_type_t t) {
    switch (t) {
    case COS_V245_TYPE_GAUGE:     return "gauge";
    case COS_V245_TYPE_COUNTER:   return "counter";
    case COS_V245_TYPE_HISTOGRAM: return "histogram";
    }
    return "unknown";
}

size_t cos_v245_to_json(const cos_v245_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v245\","
        "\"n_metrics\":%d,\"n_metric_names_valid\":%d,"
        "\"scrape_endpoint\":\"%s\","
        "\"n_log_fields\":%d,\"n_log_levels\":%d,"
        "\"n_spans\":%d,\"sigma_trace\":%.4f,"
        "\"sigma_product_request\":%.4f,\"total_latency_ms\":%d,"
        "\"n_alerts\":%d,\"tau_alert\":%.3f,\"k_crit\":%.3f,"
        "\"sigma_observe\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"metrics\":[",
        COS_V245_N_METRICS, s->n_metric_names_valid,
        s->scrape_endpoint,
        COS_V245_N_LOG_FIELDS, COS_V245_N_LOG_LEVELS,
        s->n_spans, s->sigma_trace,
        s->sigma_product_request, s->total_latency_ms,
        COS_V245_N_ALERTS, s->tau_alert, s->k_crit,
        s->sigma_observe,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V245_N_METRICS; ++i) {
        const cos_v245_metric_t *m = &s->metrics[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"type\":\"%s\","
            "\"help\":\"%s\",\"name_valid\":%s}",
            i == 0 ? "" : ",",
            m->name, type_name(m->type), m->help,
            m->name_valid ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int q = snprintf(buf + off, cap - off, "],\"log_fields\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V245_N_LOG_FIELDS; ++i) {
        int z = snprintf(buf + off, cap - off, "%s\"%s\"",
                         i == 0 ? "" : ",", s->log_fields[i]);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"log_levels\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V245_N_LOG_LEVELS; ++i) {
        int z = snprintf(buf + off, cap - off, "%s\"%s\"",
                         i == 0 ? "" : ",", s->log_levels[i]);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"spans\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V245_N_TRACE_SPANS; ++i) {
        const cos_v245_span_t *sp = &s->spans[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"sigma\":%.4f,\"tick\":%d}",
            i == 0 ? "" : ",", sp->name, sp->sigma, sp->tick);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"alerts\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V245_N_ALERTS; ++i) {
        const cos_v245_alert_t *a = &s->alerts[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"id\":\"%s\",\"condition\":\"%s\","
            "\"n_channels\":%d,\"channels\":[",
            i == 0 ? "" : ",", a->id, a->condition, a->n_channels);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
        for (int k = 0; k < a->n_channels; ++k) {
            int zz = snprintf(buf + off, cap - off, "%s\"%s\"",
                              k == 0 ? "" : ",", a->channels[k]);
            if (zz < 0 || off + (size_t)zz >= cap) return 0;
            off += (size_t)zz;
        }
        int m = snprintf(buf + off, cap - off, "]}");
        if (m < 0 || off + (size_t)m >= cap) return 0;
        off += (size_t)m;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v245_self_test(void) {
    cos_v245_state_t s;
    cos_v245_init(&s, 0x2450B5E7ULL);
    cos_v245_run(&s);
    if (!s.chain_valid) return 1;

    const char *mnames[COS_V245_N_METRICS] = {
        "cos_sigma_product", "cos_k_eff", "cos_requests_total",
        "cos_latency_seconds", "cos_tokens_per_second",
        "cos_kernels_active", "cos_abstain_rate"
    };
    for (int i = 0; i < COS_V245_N_METRICS; ++i) {
        if (strcmp(s.metrics[i].name, mnames[i]) != 0) return 2;
        if (!s.metrics[i].name_valid) return 3;
        if (s.metrics[i].type != COS_V245_TYPE_GAUGE &&
            s.metrics[i].type != COS_V245_TYPE_COUNTER &&
            s.metrics[i].type != COS_V245_TYPE_HISTOGRAM) return 4;
    }
    if (s.n_metric_names_valid != COS_V245_N_METRICS) return 5;
    if (strcmp(s.scrape_endpoint, "GET /metrics") != 0) return 6;

    const char *want_fields[COS_V245_N_LOG_FIELDS] = {
        "id","timestamp","level","sigma_product",
        "latency_ms","kernels_used","pipeline_type","presence_state"
    };
    for (int i = 0; i < COS_V245_N_LOG_FIELDS; ++i)
        if (strcmp(s.log_fields[i], want_fields[i]) != 0) return 7;
    const char *want_levels[COS_V245_N_LOG_LEVELS] = {
        "DEBUG","INFO","WARN","ERROR"
    };
    for (int i = 0; i < COS_V245_N_LOG_LEVELS; ++i)
        if (strcmp(s.log_levels[i], want_levels[i]) != 0) return 8;

    if (s.n_spans < 3) return 9;
    float max_sigma = 0.0f;
    for (int i = 0; i < s.n_spans; ++i) {
        if (s.spans[i].sigma < 0.0f || s.spans[i].sigma > 1.0f) return 10;
        if (s.spans[i].sigma > max_sigma) max_sigma = s.spans[i].sigma;
        if (i > 0 && s.spans[i].tick <= s.spans[i - 1].tick) return 11;
    }
    float diff = s.sigma_trace - max_sigma;
    if (diff < 0.0f) diff = -diff;
    if (diff > 1e-6f) return 12;

    const char *want_ids[COS_V245_N_ALERTS] = { "A1", "A2", "A3", "A4" };
    for (int i = 0; i < COS_V245_N_ALERTS; ++i) {
        if (strcmp(s.alerts[i].id, want_ids[i]) != 0) return 13;
        if (strlen(s.alerts[i].condition) == 0) return 14;
        if (s.alerts[i].n_channels < 1) return 15;
    }

    if (s.sigma_observe < 0.0f || s.sigma_observe > 1.0f) return 16;
    if (s.sigma_observe > 1e-6f) return 17;

    cos_v245_state_t t;
    cos_v245_init(&t, 0x2450B5E7ULL);
    cos_v245_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 18;
    return 0;
}
