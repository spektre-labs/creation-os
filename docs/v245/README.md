# v245 — σ-Observe (`docs/v245/`)

Production observability for Creation OS.  v160 telemetry is the
internal tape; v245 is the production-grade outside: Prometheus
metrics, structured logs, OpenTelemetry traces, and an alerting
manifest — all typed, all audited, all byte-deterministic.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Prometheus metrics (exactly 7)

| name                    | type      | help                         |
|-------------------------|-----------|------------------------------|
| `cos_sigma_product`     | gauge     | per-request σ_product        |
| `cos_k_eff`             | gauge     | system-wide `K_eff`          |
| `cos_requests_total`    | counter   | total requests served        |
| `cos_latency_seconds`   | histogram | request latency seconds      |
| `cos_tokens_per_second` | gauge     | tokens per second            |
| `cos_kernels_active`    | gauge     | kernels currently active     |
| `cos_abstain_rate`      | gauge     | abstain rate in `[0, 1]`     |

Scrape endpoint: `GET /metrics`.

### Structured log schema (8 required fields)

```
id, timestamp, level, sigma_product,
latency_ms, kernels_used, pipeline_type, presence_state
```

Levels: `DEBUG`, `INFO`, `WARN`, `ERROR` — rising σ auto-promotes
the level to `WARN` at the edge.

### OpenTelemetry trace (v0 fixture)

```
reason (σ=0.18) → recall (σ=0.15) → verify (σ=0.21)
→ reflect (σ=0.23) → emit (σ=0.12)

σ_product request = 0.23, total latency = 340 ms
```

Ticks strictly ascending; `σ_trace = max span σ` is the bottleneck
of the chain.

### Alert rules (exactly 4)

| id  | condition                               | channels               |
|-----|-----------------------------------------|------------------------|
| A1  | `σ_product > τ_alert (0.60)`            | webhook · email · slack |
| A2  | `K_eff < K_crit (0.70)`                 | webhook · email · slack |
| A3  | `abstain_rate > 0.30`                   | webhook · email · slack |
| A4  | `guardian_anomaly == true (v210)`       | webhook · email · slack |

### σ_observe

```
σ_observe = 1 − valid_metric_names / n_metrics
```

v0 requires `σ_observe == 0.0`: every metric name is a valid
Prometheus identifier and every log field is present.

## Merge-gate contract

`bash benchmarks/v245/check_v245_observe_metrics_endpoint.sh`

- self-test PASSES
- exactly 7 metrics in canonical order; every name valid
  Prometheus; type ∈ `{gauge, counter, histogram}`
- `scrape_endpoint == "GET /metrics"`
- 8 log fields in order; 4 log levels in order
- trace ≥ 3 spans with strictly-ascending ticks,
  `σ_trace == max span σ`
- exactly 4 alert rules `A1..A4`; each non-empty condition and
  ≥ 1 channel
- `σ_observe ∈ [0, 1]` AND `σ_observe == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed manifests for metrics / logs /
  trace / alerts with audit predicates and FNV-1a chain.
- **v245.1 (named, not implemented)** — live Prometheus pull,
  Jaeger / Zipkin push, Slack webhook wire-up, alert silencing
  / inhibition, SLO burn-rate metrics.

## Honest claims

- **Is:** a typed, falsifiable observability surface manifest.
- **Is not:** a running exporter.  v245.1 is where the manifest
  drives a real `/metrics` handler and a live trace exporter.

---

*Spektre Labs · Creation OS · 2026*
