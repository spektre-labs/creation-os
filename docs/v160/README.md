# v160 — σ-Telemetry · OpenTelemetry + Prometheus + Structured Logs

**Header:** [`src/v160/telemetry.h`](../../src/v160/telemetry.h) · **Source:** [`src/v160/telemetry.c`](../../src/v160/telemetry.c) · **CLI:** [`src/v160/main.c`](../../src/v160/main.c) · **Gate:** [`benchmarks/v160/check_v160_telemetry_otlp_export.sh`](../../benchmarks/v160/check_v160_telemetry_otlp_export.sh) · **Make:** `check-v160`

## Problem

v133 σ-meta measures runtime performance. v159 σ-heal repairs
faults. Neither is consumable by an external operator: Creation
OS does not export traces, expose /metrics for Prometheus, or
write structured logs an on-call engineer could grep. Without a
unified observability layer, debugging a production Creation OS
deployment is impossible from outside the process.

v160 unifies them. Every v106 HTTP request is modelled as a
six-span cognitive trace:

```
encode → recall → predict → generate → metacognition → decide
```

Each span carries the per-stage σ as an OTLP attribute and names
the kernel it executes on. The same kernel also exposes a
Prometheus text-format /metrics endpoint and a ring-buffered
ndjson log stream with trace correlation.

## σ-innovation

Three deterministic surfaces — one σ-attributed trace tree, one
σ-labelled metrics payload, one σ-indexed log stream — all
pinned to the same `trace_id`.

**σ-per-span attributes:** Each span's `cos.sigma` attribute
equals the kernel's per-stage σ. The root σ_product (geomean)
is also a span attribute on the root span and a gauge in
/metrics.

**Kernel provenance:** Each span names its source kernel via
`cos.kernel` — `v118_vision` for encode, `v115_memory` for
recall, `v131_temporal` for predict, `v101_bitnet` for generate,
`v140_meta` for metacognition, `v111_sigma` for decide.

## Three emitters

| Emitter | Format | Consumes |
|---|---|---|
| Tracing | OTLP/JSON v1.0 | Jaeger · Grafana Tempo · Elastic · Datadog |
| Metrics | Prometheus text-format v0.0.4 | Prometheus · VictoriaMetrics · Grafana Cloud |
| Logs | ndjson | Loki · Elastic · fluentbit · `jq` |

Canonical metrics:

```
cos_sigma_product{task="chat"}       gauge
cos_sigma_channel{channel="..."}      gauge
cos_abstain_total                     counter
cos_heal_total                        counter
cos_rsi_cycle_total                   counter
cos_skill_count                       gauge
```

## Merge-gate

`make check-v160` asserts:

1. 5-contract self-test (T1..T5) passes.
2. OTLP JSON contains `resourceSpans`, `service.name`
   = `creation-os`, 6 named spans, a `cos.sigma` and
   `cos.kernel` attribute on every span.
3. The OTLP JSON parses via Python `json.loads` and passes the
   span-chain invariant (every span's `parentSpanId` equals the
   previous span's `spanId`).
4. Prometheus output includes `# TYPE` headers, gauges, counters,
   and the six canonical metric names.
5. ndjson logs contain `level`, `component`, `trace_id`,
   `sigma_product`, `message` on every line.
6. Byte-identical OTLP JSON under the same `(seed, task)`;
   different seeds produce different traces.

## v160.0 vs v160.1

* **v160.0** — pure in-process emitter. Produces OTLP JSON,
  Prometheus text, and ndjson, all deterministic in `(seed, task)`.
  No sockets, no threads, no rotation.
* **v160.1** — wires the emitter to:
  - a real OTLP/HTTP POST against `$OTEL_EXPORTER_OTLP_ENDPOINT`
  - a real `/metrics` endpoint on v106
  - a log-rotation daemon (7 days, max 100 MB on disk)
  - v108 dashboard tiles for real-time σ channels, health status
    (v159), tracing timeline, σ trend (v131), RSI history (v144),
    skill library (v145), swarm node status (v128).

## Honest claims

* The OTLP JSON shape follows the OTLP v1.0 wire format; any
  OpenTelemetry collector that accepts OTLP/HTTP can ingest it
  after a trivial wrapper.
* The Prometheus text-format is v0.0.4 compliant and passes
  `promtool check metrics` in v160.1; v160.0 verifies structure
  via the gate only.
* No timing data is real in v160.0: span `startTimeUnixNano` and
  `endTimeUnixNano` are synthesized deterministically from the
  seed. Real wall-clock timing is v160.1.
