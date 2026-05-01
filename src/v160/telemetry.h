/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v160 σ-Telemetry — OpenTelemetry + Prometheus + structured-log
 * observability kernel.
 *
 * v133 σ-meta measures runtime performance.  v159 σ-heal repairs
 * faults.  v160 unifies them into an observability layer:
 *
 *   1. OpenTelemetry-compatible tracing.  Each v106 HTTP request
 *      fans out into a 6-span cognitive trace
 *        [encode → recall → predict → generate → metacognition → decide]
 *      and each span carries the per-stage σ profile as a span
 *      attribute.  The emitter produces OTLP-JSON (OTLP/gRPC and
 *      OTLP/HTTP both consume JSON over the wire when
 *      OTEL_EXPORTER_OTLP_PROTOCOL=http/json), which Jaeger,
 *      Grafana Tempo, and Elastic all ingest.
 *
 *   2. Prometheus-compatible /metrics endpoint.  Canonical
 *      expo-format gauges + counters:
 *        cos_sigma_product{task="chat"}    gauge
 *        cos_sigma_channel{channel="..."}   gauge
 *        cos_abstain_total                   counter
 *        cos_heal_total                      counter
 *        cos_rsi_cycle_total                 counter
 *        cos_skill_count                     gauge
 *
 *   3. Structured JSON logs.  One JSON object per event with
 *      {level, timestamp, component, σ_product, message,
 *      trace_id}.  Deterministic in (seed, event-count).
 *
 * v160.0 (this file) is a pure C11 emitter: it takes an in-memory
 * request → produces OTLP-JSON + Prometheus text + per-event JSON
 * logs, with a bounded ring buffer of ≤256 log lines.  No sockets,
 * no background threads.
 *
 * v160.1 (named, not shipped) wires the emitter to:
 *   - a real OTLP/HTTP POST against $OTEL_EXPORTER_OTLP_ENDPOINT
 *   - a real /metrics endpoint on v106
 *   - a log-rotation daemon (7 days, max 100MB)
 *   - the v108 dashboard extended with tracing/health/trend tiles
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V160_TELEMETRY_H
#define COS_V160_TELEMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V160_MAX_SPANS   6
#define COS_V160_MAX_NAME    32
#define COS_V160_MAX_MSG     128
#define COS_V160_MAX_LOGS    256
#define COS_V160_MAX_METRICS 16

/* ---------- tracing -------------------------------------------- */

typedef enum {
    COS_V160_SPAN_ENCODE        = 0,
    COS_V160_SPAN_RECALL        = 1,
    COS_V160_SPAN_PREDICT       = 2,
    COS_V160_SPAN_GENERATE      = 3,
    COS_V160_SPAN_METACOGNITION = 4,
    COS_V160_SPAN_DECIDE        = 5,
} cos_v160_span_kind_t;

typedef struct {
    cos_v160_span_kind_t kind;
    char   name[COS_V160_MAX_NAME];
    /* Hex-encoded (16 hex chars) span ids for OTLP compatibility. */
    char   span_id[17];
    char   parent_id[17];
    /* Start/end wall-clock nanoseconds — synthesized deterministically. */
    uint64_t start_unix_nano;
    uint64_t end_unix_nano;
    float  sigma;            /* per-stage σ */
    char   kernel[COS_V160_MAX_NAME]; /* e.g. "v111", "v115" */
    char   status[COS_V160_MAX_NAME]; /* "OK" | "ERROR" */
} cos_v160_span_t;

typedef struct {
    /* Hex-encoded (32 hex chars) trace id. */
    char                 trace_id[33];
    char                 root_span_id[17];
    uint64_t             seed;
    int                  n_spans;
    cos_v160_span_t      spans[COS_V160_MAX_SPANS];
    float                sigma_product;       /* geomean over spans */
    const char          *task;                /* static literal */
} cos_v160_trace_t;

/* Build a deterministic 6-span chat-style trace under `seed`.
 * `task` must be a static string (e.g. "chat").  Fills in trace_id,
 * span_ids, timestamps, σ per stage, σ_product.  Idempotent: same
 * seed + task → byte-identical output. */
void cos_v160_trace_init(cos_v160_trace_t *t, uint64_t seed, const char *task);

/* Serialize the trace to OTLP-JSON (resource + scope + spans tree).
 * Returns bytes written (excluding NUL). */
size_t cos_v160_trace_to_otlp_json(const cos_v160_trace_t *t,
                                   char *buf, size_t cap);

/* ---------- metrics -------------------------------------------- */

typedef enum {
    COS_V160_METRIC_GAUGE   = 0,
    COS_V160_METRIC_COUNTER = 1,
} cos_v160_metric_kind_t;

typedef struct {
    cos_v160_metric_kind_t kind;
    char   name[COS_V160_MAX_NAME];
    char   label_key[COS_V160_MAX_NAME];  /* "" for unlabeled */
    char   label_val[COS_V160_MAX_NAME];
    double value;
} cos_v160_metric_t;

typedef struct {
    int                n_metrics;
    cos_v160_metric_t  metrics[COS_V160_MAX_METRICS];
} cos_v160_registry_t;

void cos_v160_registry_init(cos_v160_registry_t *r);

/* Append (or update for gauges, increment for counters) a metric. */
int cos_v160_registry_put(cos_v160_registry_t *r,
                          cos_v160_metric_kind_t kind,
                          const char *name,
                          const char *label_key,
                          const char *label_val,
                          double value);

/* Emit Prometheus text-format v0.0.4 to `buf`.  Returns bytes
 * written (excluding NUL). */
size_t cos_v160_registry_to_prometheus(const cos_v160_registry_t *r,
                                       char *buf, size_t cap);

/* ---------- logging -------------------------------------------- */

typedef enum {
    COS_V160_LOG_INFO  = 0,
    COS_V160_LOG_WARN  = 1,
    COS_V160_LOG_ERROR = 2,
} cos_v160_log_level_t;

typedef struct {
    cos_v160_log_level_t level;
    uint64_t             timestamp_unix_nano;
    char                 component[COS_V160_MAX_NAME];
    char                 trace_id[33];
    float                sigma_product;
    char                 message[COS_V160_MAX_MSG];
} cos_v160_log_t;

typedef struct {
    int            n_logs;
    cos_v160_log_t logs[COS_V160_MAX_LOGS];
} cos_v160_log_ring_t;

void cos_v160_log_ring_init(cos_v160_log_ring_t *lr);

int cos_v160_log_ring_push(cos_v160_log_ring_t *lr,
                           cos_v160_log_level_t level,
                           const char *component,
                           const char *trace_id,
                           float       sigma_product,
                           const char *message,
                           uint64_t    timestamp);

/* Emit newline-delimited JSON (ndjson).  Returns bytes written. */
size_t cos_v160_log_ring_to_ndjson(const cos_v160_log_ring_t *lr,
                                   char *buf, size_t cap);

/* ---------- self-test ------------------------------------------ */

int cos_v160_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V160_TELEMETRY_H */
