/*
 * v245 σ-Observe — production observability.
 *
 *   v160 telemetry is the internal side.  v245 is the
 *   production-grade outside:
 *     1. Prometheus-compatible metrics
 *     2. structured JSON logs
 *     3. OpenTelemetry-compatible traces
 *     4. alerting rules
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Metrics (exactly 7, canonical order):
 *     cos_sigma_product    gauge     per-request σ_product
 *     cos_k_eff            gauge     coherence K_eff
 *     cos_requests_total   counter   monotonically increasing
 *     cos_latency_seconds  histogram request latency distribution
 *     cos_tokens_per_second gauge    tok/s throughput
 *     cos_kernels_active   gauge     how many kernels live now
 *     cos_abstain_rate     gauge     fraction in [0, 1]
 *   Scrape endpoint declared: GET /metrics.
 *
 *   Structured log schema (exactly 8 required fields):
 *     id, timestamp, level, sigma_product, latency_ms,
 *     kernels_used, pipeline_type, presence_state
 *   Log levels (exactly 4): DEBUG, INFO, WARN, ERROR.
 *
 *   OpenTelemetry trace (v0 fixture, 5 spans):
 *     reason → recall → verify → reflect → emit
 *   Ticks strictly ascending; each span carries a σ;
 *   `sigma_trace = max span σ`; fixture σ_product =
 *   0.23, total latency 340 ms (matches the user story
 *   in the v245 spec).
 *
 *   Alert rules (exactly 4, canonical order):
 *     A1: σ_product  > τ_alert  (0.60)
 *     A2: K_eff      < K_crit   (0.70)
 *     A3: abstain_rate > 0.30
 *     A4: guardian_anomaly == true (v210)
 *   Channels: webhook + email + slack (all three).
 *
 *   σ_observe (surface hygiene):
 *       σ_observe = 1 − valid_metric_names /
 *                       n_metrics
 *   v0 requires `σ_observe == 0.0`: every metric name
 *   is a valid Prometheus identifier (starts with
 *   letter or '_', all subsequent chars are
 *   alphanumeric or '_' or ':'), and every log field
 *   is present.
 *
 *   Contracts (v0):
 *     1. n_metrics == 7; names match the manifest in
 *        canonical order; every metric has type ∈
 *        {gauge, counter, histogram}.
 *     2. Scrape endpoint == "GET /metrics".
 *     3. Log schema has all 8 required fields.
 *     4. Log levels == {DEBUG, INFO, WARN, ERROR} in
 *        that order.
 *     5. Trace has ≥ 3 spans, strictly ascending
 *        ticks, per-span σ ∈ [0, 1],
 *        `sigma_trace == max span σ`.
 *     6. Exactly 4 alert rules in canonical order;
 *        every rule has a non-empty `condition` and at
 *        least one channel.
 *     7. σ_observe ∈ [0, 1] AND σ_observe == 0.0.
 *     8. FNV-1a chain replays byte-identically.
 *
 *   v245.1 (named, not implemented): live Prometheus
 *     pull, Jaeger / Zipkin push, Slack webhook wire-
 *     up, alert silencing and inhibition, SLO burn
 *     rate metrics.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V245_OBSERVE_H
#define COS_V245_OBSERVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V245_N_METRICS     7
#define COS_V245_N_LOG_FIELDS  8
#define COS_V245_N_LOG_LEVELS  4
#define COS_V245_N_TRACE_SPANS 5
#define COS_V245_N_ALERTS      4
#define COS_V245_N_CHANNELS    3

typedef enum {
    COS_V245_TYPE_GAUGE     = 1,
    COS_V245_TYPE_COUNTER   = 2,
    COS_V245_TYPE_HISTOGRAM = 3
} cos_v245_metric_type_t;

typedef struct {
    char                   name[40];
    cos_v245_metric_type_t type;
    char                   help[64];
    bool                   name_valid;
} cos_v245_metric_t;

typedef struct {
    char    name[24];
    float   sigma;
    int     tick;
} cos_v245_span_t;

typedef struct {
    char    id[4];         /* "A1" .. "A4" */
    char    condition[48];
    char    channels[COS_V245_N_CHANNELS][16]; /* webhook/email/slack */
    int     n_channels;
} cos_v245_alert_t;

typedef struct {
    cos_v245_metric_t metrics[COS_V245_N_METRICS];
    int               n_metric_names_valid;
    char              scrape_endpoint[24];  /* "GET /metrics" */

    char              log_fields[COS_V245_N_LOG_FIELDS][24];
    char              log_levels[COS_V245_N_LOG_LEVELS][8];

    cos_v245_span_t   spans[COS_V245_N_TRACE_SPANS];
    int               n_spans;
    float             sigma_trace;
    float             sigma_product_request;
    int               total_latency_ms;

    cos_v245_alert_t  alerts[COS_V245_N_ALERTS];
    float             tau_alert;
    float             k_crit;

    float             sigma_observe;

    bool              chain_valid;
    uint64_t          terminal_hash;
    uint64_t          seed;
} cos_v245_state_t;

void   cos_v245_init(cos_v245_state_t *s, uint64_t seed);
void   cos_v245_run (cos_v245_state_t *s);

size_t cos_v245_to_json(const cos_v245_state_t *s,
                         char *buf, size_t cap);

int    cos_v245_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V245_OBSERVE_H */
