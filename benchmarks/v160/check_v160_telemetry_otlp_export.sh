#!/usr/bin/env bash
# v160 σ-Telemetry merge-gate: proves the emitter produces a
# well-formed OTLP-JSON trace (Jaeger/Grafana/Elastic-ingestible),
# a Prometheus /metrics text-format payload with all required
# names, a ndjson log stream with σ_product + trace_id, and that
# the whole thing is deterministic in (seed, task).
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail

BIN=./creation_os_v160_telemetry
SEED=160

die() { echo "v160 check: FAIL — $*" >&2; exit 1; }

echo "  self-test"
st="$($BIN --self-test)"
echo "  $st"
echo "$st" | grep -q '"self_test":"pass"' || die "self-test"

echo "  trace → OTLP JSON"
tr="$($BIN --trace --seed $SEED --task chat)"
echo "$tr" | grep -q '"resourceSpans"'                   || die "no resourceSpans"
echo "$tr" | grep -q '"service.name"'                    || die "no service.name"
echo "$tr" | grep -q '"stringValue":"creation-os"'       || die "service.name value"
echo "$tr" | grep -q '"name":"encode"'                   || die "no encode span"
echo "$tr" | grep -q '"name":"recall"'                   || die "no recall span"
echo "$tr" | grep -q '"name":"predict"'                  || die "no predict span"
echo "$tr" | grep -q '"name":"generate"'                 || die "no generate span"
echo "$tr" | grep -q '"name":"metacognition"'            || die "no metacognition span"
echo "$tr" | grep -q '"name":"decide"'                   || die "no decide span"
echo "$tr" | grep -q '"cos.sigma"'                       || die "no σ attribute"
echo "$tr" | grep -q '"cos.kernel"'                      || die "no kernel attribute"
echo "$tr" | grep -q '"stringValue":"v101_bitnet"'       || die "generate must point at v101"

echo "  JSON structural (python parses)"
python3 - <<'PY' <<<"$tr"
import json, sys
doc = json.loads(sys.stdin.read())
rs  = doc["resourceSpans"]
assert len(rs) == 1, rs
spans = rs[0]["scopeSpans"][0]["spans"]
assert len(spans) == 6, len(spans)
for s in spans:
    assert "traceId" in s and len(s["traceId"]) == 32, s
    assert "spanId"  in s and len(s["spanId"])  == 16, s
    # parentSpanId is 16 hex chars too
    assert len(s["parentSpanId"]) == 16, s
# Chain: span[i].parentSpanId == span[i-1].spanId for i>=1
for i in range(1, 6):
    assert spans[i]["parentSpanId"] == spans[i-1]["spanId"], (i, spans[i-1], spans[i])
# First span parent == root span id (not present in span list, but non-empty).
assert spans[0]["parentSpanId"] != spans[0]["spanId"]
print("otlp-json ok: 6 spans linearly parented under a single root")
PY

echo "  metrics → Prometheus text format"
mp="$($BIN --metrics --seed $SEED)"
echo "$mp" | grep -q "^# TYPE cos_sigma_product gauge"  || die "missing gauge header"
echo "$mp" | grep -q "^# TYPE cos_abstain_total counter" || die "missing counter header"
for name in cos_sigma_product cos_sigma_channel cos_abstain_total \
            cos_heal_total cos_rsi_cycle_total cos_skill_count; do
    echo "$mp" | grep -qE "^${name}[ {]" || die "missing metric $name"
done
echo "$mp" | grep -q 'cos_sigma_channel{channel="generate"}' || die "generate label"

echo "  logs → ndjson with σ_product + trace_id"
logs="$($BIN --logs --seed $SEED --task chat)"
# Every line must parse as JSON and carry level/component/trace_id/σ.
while IFS= read -r line; do
    [ -z "$line" ] && continue
    python3 -c "import json,sys; o=json.loads(sys.argv[1]); \
        assert 'level' in o and 'component' in o and 'trace_id' in o \
        and 'sigma_product' in o and 'message' in o, o" "$line" \
        || die "bad log line: $line"
done <<<"$logs"
echo "$logs" | grep -q '"component":"v106"'        || die "no v106 log"
echo "$logs" | grep -q '"component":"v101_bitnet"' || die "no v101_bitnet log"

echo "  determinism (same seed/task → byte-identical OTLP)"
a="$($BIN --trace --seed $SEED --task chat)"
b="$($BIN --trace --seed $SEED --task chat)"
[ "$a" = "$b" ] || die "non-deterministic"
c="$($BIN --trace --seed 999 --task chat)"
[ "$a" != "$c" ] || die "different seeds must produce different traces"

echo "v160 telemetry otlp-export: OK"
