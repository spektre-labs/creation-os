#!/usr/bin/env bash
#
# v245 σ-Observe — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * exactly 7 metrics in canonical order, each with a valid
#     Prometheus name and type in {gauge, counter, histogram}
#   * scrape endpoint == "GET /metrics"
#   * log schema has 8 required fields in order:
#     id, timestamp, level, sigma_product, latency_ms,
#     kernels_used, pipeline_type, presence_state
#   * log levels in order: DEBUG, INFO, WARN, ERROR
#   * trace has >=3 spans with strictly ascending ticks,
#     per-span sigma in [0,1], sigma_trace == max span sigma
#   * exactly 4 alerts (A1..A4), each non-empty condition
#     and >=1 channel (webhook/email/slack manifest)
#   * sigma_observe in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v245_observe"
[ -x "$BIN" ] || { echo "v245: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json, re
d = json.loads("""$OUT""")
assert d["kernel"] == "v245", d
assert d["chain_valid"] is True, d

want_metrics = [
    "cos_sigma_product","cos_k_eff","cos_requests_total",
    "cos_latency_seconds","cos_tokens_per_second",
    "cos_kernels_active","cos_abstain_rate",
]
names = [m["name"] for m in d["metrics"]]
assert names == want_metrics, names
prom_re = re.compile(r"^[A-Za-z_][A-Za-z0-9_:]*$")
for m in d["metrics"]:
    assert m["type"] in ("gauge","counter","histogram"), m
    assert prom_re.match(m["name"]), m
    assert m["name_valid"] is True, m
assert d["scrape_endpoint"] == "GET /metrics", d

want_fields = ["id","timestamp","level","sigma_product",
               "latency_ms","kernels_used","pipeline_type",
               "presence_state"]
assert d["log_fields"] == want_fields, d
assert d["log_levels"] == ["DEBUG","INFO","WARN","ERROR"], d

spans = d["spans"]
assert len(spans) >= 3, spans
for i, sp in enumerate(spans):
    assert 0.0 <= sp["sigma"] <= 1.0, sp
    if i > 0:
        assert sp["tick"] > spans[i-1]["tick"], spans
assert abs(d["sigma_trace"] - max(s["sigma"] for s in spans)) < 1e-6, d

alerts = d["alerts"]
assert [a["id"] for a in alerts] == ["A1","A2","A3","A4"], alerts
for a in alerts:
    assert len(a["condition"]) > 0, a
    assert a["n_channels"] == len(a["channels"]) >= 1, a

assert 0.0 <= d["sigma_observe"] <= 1.0, d
assert d["sigma_observe"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v245: non-deterministic" >&2; exit 1; }
