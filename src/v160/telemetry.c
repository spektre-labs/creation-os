/*
 * v160 σ-Telemetry — emitter implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "telemetry.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---------- deterministic RNG ---------------------------------- */

static uint64_t sm(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float urand(uint64_t *s) {
    return (float)((sm(s) >> 40) / (float)(1ULL << 24));
}

static void copy_bounded(char *dst, const char *src, size_t cap) {
    if (!dst || cap == 0) return;
    size_t n = 0;
    if (src) { while (src[n] && n + 1 < cap) { dst[n] = src[n]; ++n; } }
    dst[n] = '\0';
}

static void hex_from_u64(uint64_t x, char *out) {
    static const char H[] = "0123456789abcdef";
    for (int i = 0; i < 16; ++i) {
        out[i] = H[(x >> ((15 - i) * 4)) & 0xFULL];
    }
    out[16] = '\0';
}

static void hex_from_u128(uint64_t hi, uint64_t lo, char *out) {
    hex_from_u64(hi, out);
    hex_from_u64(lo, out + 16);
    out[32] = '\0';
}

/* ---------- static span templates ------------------------------ */

typedef struct {
    const char *name;
    const char *kernel;
    /* synthetic μs latency per stage at the start */
    uint64_t    latency_us;
} v160_span_tpl_t;

static const v160_span_tpl_t SPAN_TPLS[COS_V160_MAX_SPANS] = {
    [COS_V160_SPAN_ENCODE]        = {"encode",        "v118_vision",   800},
    [COS_V160_SPAN_RECALL]        = {"recall",        "v115_memory",   400},
    [COS_V160_SPAN_PREDICT]       = {"predict",       "v131_temporal", 300},
    [COS_V160_SPAN_GENERATE]      = {"generate",      "v101_bitnet",  4200},
    [COS_V160_SPAN_METACOGNITION] = {"metacognition", "v140_meta",     250},
    [COS_V160_SPAN_DECIDE]        = {"decide",        "v111_sigma",    120},
};

/* ---------- trace ---------------------------------------------- */

void cos_v160_trace_init(cos_v160_trace_t *t, uint64_t seed,
                         const char *task) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
    t->seed    = seed ? seed : 0x60EEE10160ULL;
    t->task    = task ? task : "chat";
    t->n_spans = COS_V160_MAX_SPANS;

    uint64_t prng = seed ? seed : 0xA160A160A160A160ULL;
    uint64_t tr_hi = sm(&prng);
    uint64_t tr_lo = sm(&prng);
    hex_from_u128(tr_hi, tr_lo, t->trace_id);

    uint64_t root_id = sm(&prng);
    hex_from_u64(root_id, t->root_span_id);

    uint64_t now_ns = 1700000000ULL * 1000000000ULL
                    + (seed & 0xFFFFFFFFULL) * 1000ULL;
    double log_sum = 0.0;

    for (int i = 0; i < COS_V160_MAX_SPANS; ++i) {
        cos_v160_span_t *s = &t->spans[i];
        s->kind = (cos_v160_span_kind_t)i;
        copy_bounded(s->name,   SPAN_TPLS[i].name,   sizeof(s->name));
        copy_bounded(s->kernel, SPAN_TPLS[i].kernel, sizeof(s->kernel));

        uint64_t span_id = sm(&prng);
        hex_from_u64(span_id, s->span_id);
        /* Linear chain: previous span is parent (root for first). */
        copy_bounded(s->parent_id,
                     (i == 0) ? t->root_span_id : t->spans[i - 1].span_id,
                     sizeof(s->parent_id));

        s->start_unix_nano = now_ns;
        uint64_t dur_ns = (uint64_t)SPAN_TPLS[i].latency_us * 1000ULL;
        dur_ns += (uint64_t)((urand(&prng) * 500.0f) * 1000.0f);
        s->end_unix_nano = now_ns + dur_ns;
        now_ns = s->end_unix_nano;

        /* σ per span: base 0.10..0.30 + jitter. */
        float base = 0.10f + 0.04f * (float)i;
        s->sigma   = base + (urand(&prng) - 0.5f) * 0.05f;
        if (s->sigma < 0.0f) s->sigma = 0.0f;
        if (s->sigma > 1.0f) s->sigma = 1.0f;

        copy_bounded(s->status, "OK", sizeof(s->status));
        log_sum += log((double)(s->sigma > 1e-6f ? s->sigma : 1e-6f));
    }
    t->sigma_product = (float)exp(log_sum / (double)COS_V160_MAX_SPANS);
}

/* Small JSON builder. */
static int jprintf(char *buf, size_t cap, size_t *off,
                   const char *fmt, ...) {
    if (!buf || *off >= cap) return 0;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    if ((size_t)n >= cap - *off) { *off = cap - 1; return 0; }
    *off += (size_t)n;
    return 1;
}

size_t cos_v160_trace_to_otlp_json(const cos_v160_trace_t *t,
                                   char *buf, size_t cap) {
    if (!t || !buf || cap == 0) return 0;
    size_t off = 0;

    /* Outer resource spans array, following OTLP JSON v1.0 shape. */
    jprintf(buf, cap, &off, "{\"resourceSpans\":[{");
    jprintf(buf, cap, &off,
        "\"resource\":{\"attributes\":["
          "{\"key\":\"service.name\","
            "\"value\":{\"stringValue\":\"creation-os\"}},"
          "{\"key\":\"service.version\","
            "\"value\":{\"stringValue\":\"v1.0.0\"}}"
        "]},");
    jprintf(buf, cap, &off,
        "\"scopeSpans\":[{\"scope\":{\"name\":\"cos.v160\"},\"spans\":[");
    for (int i = 0; i < t->n_spans; ++i) {
        const cos_v160_span_t *s = &t->spans[i];
        if (i) jprintf(buf, cap, &off, ",");
        jprintf(buf, cap, &off,
          "{\"traceId\":\"%s\",\"spanId\":\"%s\",\"parentSpanId\":\"%s\","
          "\"name\":\"%s\","
          "\"startTimeUnixNano\":\"%llu\",\"endTimeUnixNano\":\"%llu\","
          "\"attributes\":["
            "{\"key\":\"cos.kernel\","
              "\"value\":{\"stringValue\":\"%s\"}},"
            "{\"key\":\"cos.task\","
              "\"value\":{\"stringValue\":\"%s\"}},"
            "{\"key\":\"cos.sigma\","
              "\"value\":{\"doubleValue\":%.4f}}"
          "],"
          "\"status\":{\"code\":1}}",
          t->trace_id, s->span_id, s->parent_id, s->name,
          (unsigned long long)s->start_unix_nano,
          (unsigned long long)s->end_unix_nano,
          s->kernel, t->task, s->sigma);
    }
    jprintf(buf, cap, &off, "]}]}]}");
    if (off < cap) buf[off] = '\0';
    return off;
}

/* ---------- metrics -------------------------------------------- */

void cos_v160_registry_init(cos_v160_registry_t *r) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
}

static int find_metric(cos_v160_registry_t *r,
                       const char *name,
                       const char *lk, const char *lv) {
    for (int i = 0; i < r->n_metrics; ++i) {
        const cos_v160_metric_t *m = &r->metrics[i];
        if (strcmp(m->name, name) == 0 &&
            strcmp(m->label_key, lk ? lk : "") == 0 &&
            strcmp(m->label_val, lv ? lv : "") == 0) {
            return i;
        }
    }
    return -1;
}

int cos_v160_registry_put(cos_v160_registry_t *r,
                          cos_v160_metric_kind_t kind,
                          const char *name,
                          const char *label_key,
                          const char *label_val,
                          double value) {
    if (!r || !name) return -1;
    int idx = find_metric(r, name,
                          label_key ? label_key : "",
                          label_val ? label_val : "");
    if (idx < 0) {
        if (r->n_metrics >= COS_V160_MAX_METRICS) return -1;
        idx = r->n_metrics++;
        cos_v160_metric_t *m = &r->metrics[idx];
        m->kind = kind;
        copy_bounded(m->name, name, sizeof(m->name));
        copy_bounded(m->label_key, label_key ? label_key : "",
                     sizeof(m->label_key));
        copy_bounded(m->label_val, label_val ? label_val : "",
                     sizeof(m->label_val));
        m->value = (kind == COS_V160_METRIC_COUNTER) ? value : value;
        return idx;
    }
    cos_v160_metric_t *m = &r->metrics[idx];
    if (kind == COS_V160_METRIC_COUNTER) m->value += value;
    else                                 m->value  = value;
    return idx;
}

size_t cos_v160_registry_to_prometheus(const cos_v160_registry_t *r,
                                       char *buf, size_t cap) {
    if (!r || !buf || cap == 0) return 0;
    size_t off = 0;
    /* Emit HELP + TYPE once per (name, kind). */
    for (int i = 0; i < r->n_metrics; ++i) {
        const cos_v160_metric_t *m = &r->metrics[i];
        /* Only emit header the first time a name appears. */
        bool first = true;
        for (int j = 0; j < i; ++j) {
            if (strcmp(r->metrics[j].name, m->name) == 0) {
                first = false; break;
            }
        }
        if (first) {
            jprintf(buf, cap, &off,
                "# HELP %s creation-os σ-telemetry metric.\n", m->name);
            jprintf(buf, cap, &off,
                "# TYPE %s %s\n", m->name,
                m->kind == COS_V160_METRIC_COUNTER ? "counter" : "gauge");
        }
        if (m->label_key[0]) {
            jprintf(buf, cap, &off, "%s{%s=\"%s\"} %.6f\n",
                    m->name, m->label_key, m->label_val, m->value);
        } else {
            jprintf(buf, cap, &off, "%s %.6f\n", m->name, m->value);
        }
    }
    if (off < cap) buf[off] = '\0';
    return off;
}

/* ---------- logs ----------------------------------------------- */

void cos_v160_log_ring_init(cos_v160_log_ring_t *lr) {
    if (!lr) return;
    memset(lr, 0, sizeof(*lr));
}

int cos_v160_log_ring_push(cos_v160_log_ring_t *lr,
                           cos_v160_log_level_t level,
                           const char *component,
                           const char *trace_id,
                           float       sigma_product,
                           const char *message,
                           uint64_t    timestamp) {
    if (!lr) return -1;
    if (lr->n_logs >= COS_V160_MAX_LOGS) return -1;
    cos_v160_log_t *l = &lr->logs[lr->n_logs++];
    l->level = level;
    l->timestamp_unix_nano = timestamp;
    copy_bounded(l->component, component ? component : "", sizeof(l->component));
    copy_bounded(l->trace_id, trace_id ? trace_id : "", sizeof(l->trace_id));
    l->sigma_product = sigma_product;
    copy_bounded(l->message, message ? message : "", sizeof(l->message));
    return 0;
}

static const char *level_name(cos_v160_log_level_t l) {
    switch (l) {
        case COS_V160_LOG_INFO:  return "info";
        case COS_V160_LOG_WARN:  return "warn";
        case COS_V160_LOG_ERROR: return "error";
    }
    return "info";
}

size_t cos_v160_log_ring_to_ndjson(const cos_v160_log_ring_t *lr,
                                   char *buf, size_t cap) {
    if (!lr || !buf || cap == 0) return 0;
    size_t off = 0;
    for (int i = 0; i < lr->n_logs; ++i) {
        const cos_v160_log_t *l = &lr->logs[i];
        jprintf(buf, cap, &off,
          "{\"level\":\"%s\",\"timestamp\":%llu,\"component\":\"%s\","
          "\"trace_id\":\"%s\",\"sigma_product\":%.4f,\"message\":\"%s\"}\n",
          level_name(l->level),
          (unsigned long long)l->timestamp_unix_nano,
          l->component, l->trace_id, l->sigma_product, l->message);
    }
    if (off < cap) buf[off] = '\0';
    return off;
}

/* ---------- self-test ------------------------------------------ */

int cos_v160_self_test(void) {
    /* T1: trace has 6 spans, σ_product in [0.05, 0.25]. */
    cos_v160_trace_t t;
    cos_v160_trace_init(&t, 160, "chat");
    if (t.n_spans != 6) return 1;
    if (t.sigma_product < 0.05f || t.sigma_product > 0.25f) return 1;
    /* Root parent should equal the synthesized root_span_id. */
    if (strcmp(t.spans[0].parent_id, t.root_span_id) != 0) return 1;
    /* Chain: span[i].parent == span[i-1].id for i>=1. */
    for (int i = 1; i < t.n_spans; ++i) {
        if (strcmp(t.spans[i].parent_id, t.spans[i - 1].span_id) != 0)
            return 1;
    }

    /* T2: OTLP JSON contains required keys. */
    char buf[8192];
    size_t nj = cos_v160_trace_to_otlp_json(&t, buf, sizeof(buf));
    if (nj == 0) return 2;
    if (!strstr(buf, "\"resourceSpans\""))   return 2;
    if (!strstr(buf, "\"service.name\""))    return 2;
    if (!strstr(buf, "creation-os"))         return 2;
    if (!strstr(buf, "\"traceId\""))         return 2;
    if (!strstr(buf, "\"cos.sigma\""))       return 2;
    if (!strstr(buf, "\"encode\""))          return 2;
    if (!strstr(buf, "\"decide\""))          return 2;

    /* T3: registry gauges + counters + Prometheus emit. */
    cos_v160_registry_t r;
    cos_v160_registry_init(&r);
    cos_v160_registry_put(&r, COS_V160_METRIC_GAUGE,
        "cos_sigma_product", "task", "chat", 0.12);
    cos_v160_registry_put(&r, COS_V160_METRIC_GAUGE,
        "cos_sigma_channel", "channel", "entropy", 0.18);
    cos_v160_registry_put(&r, COS_V160_METRIC_COUNTER,
        "cos_abstain_total", "", "", 1.0);
    cos_v160_registry_put(&r, COS_V160_METRIC_COUNTER,
        "cos_abstain_total", "", "", 2.0);
    cos_v160_registry_put(&r, COS_V160_METRIC_COUNTER,
        "cos_heal_total",    "", "", 0.0);
    cos_v160_registry_put(&r, COS_V160_METRIC_COUNTER,
        "cos_rsi_cycle_total", "", "", 0.0);
    cos_v160_registry_put(&r, COS_V160_METRIC_GAUGE,
        "cos_skill_count", "", "", 42.0);
    char prom[4096];
    size_t np = cos_v160_registry_to_prometheus(&r, prom, sizeof(prom));
    if (np == 0) return 3;
    if (!strstr(prom, "# TYPE cos_sigma_product gauge"))    return 3;
    if (!strstr(prom, "# TYPE cos_abstain_total counter"))  return 3;
    if (!strstr(prom, "cos_sigma_channel{channel=\"entropy\"}")) return 3;
    /* Counter must have accumulated 3.0 (1 + 2). */
    if (!strstr(prom, "cos_abstain_total 3.000000"))        return 3;
    if (!strstr(prom, "cos_skill_count 42.000000"))         return 3;

    /* T4: log ring emits ndjson. */
    cos_v160_log_ring_t lr;
    cos_v160_log_ring_init(&lr);
    cos_v160_log_ring_push(&lr, COS_V160_LOG_INFO, "v106",
        t.trace_id, t.sigma_product, "request served", 1700000000000000000ULL);
    cos_v160_log_ring_push(&lr, COS_V160_LOG_WARN, "v159",
        t.trace_id, 0.4f, "predictive repair scheduled", 1700000001000000000ULL);
    char logs[2048];
    size_t nl = cos_v160_log_ring_to_ndjson(&lr, logs, sizeof(logs));
    if (nl == 0) return 4;
    if (!strstr(logs, "\"level\":\"info\""))   return 4;
    if (!strstr(logs, "\"component\":\"v106\"")) return 4;
    if (!strstr(logs, "\"level\":\"warn\""))   return 4;

    /* T5: determinism — two traces with the same seed + task
     * produce byte-identical OTLP JSON. */
    cos_v160_trace_t a, b;
    cos_v160_trace_init(&a, 77, "chat");
    cos_v160_trace_init(&b, 77, "chat");
    char ja[8192], jb[8192];
    cos_v160_trace_to_otlp_json(&a, ja, sizeof(ja));
    cos_v160_trace_to_otlp_json(&b, jb, sizeof(jb));
    if (strcmp(ja, jb) != 0) return 5;
    /* Different seeds must differ. */
    cos_v160_trace_t c;
    cos_v160_trace_init(&c, 99, "chat");
    char jc[8192];
    cos_v160_trace_to_otlp_json(&c, jc, sizeof(jc));
    if (strcmp(ja, jc) == 0) return 5;

    return 0;
}
