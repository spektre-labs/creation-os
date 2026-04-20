#!/usr/bin/env bash
#
# v259 σ-measurement_t — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * sizeof == 12 AND alignof == 4
#   * 3 layout rows (header/sigma/tau) at offsets 0/4/8 size 4
#   * 4 roundtrip rows all roundtrip_ok AND observed == expected
#   * 3 gate rows with verdicts 0 < 1 < 2 (total order)
#   * gate predicate is stateless (256 identical calls)
#   * 3 microbench rows: iters >= 1_000_000; mean_ns < 1e6;
#     median_ns < max_ns; min_ns > 0 (proves a real distribution)
#   * sigma_measurement in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#     (the bench numbers differ, but structural fields do not;
#     we verify a deterministic digest of the structural subset)
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v259_sigma_measurement"
[ -x "$BIN" ] || { echo "v259: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])

assert d["kernel"] == "v259_sigma_measurement", d
assert d["sizeof"] == 12, d
assert d["alignof"] == 4, d
assert d["layout_size_ok"] is True, d
assert d["layout_align_ok"] is True, d

layout = d["layout"]
assert len(layout) == 3, layout
want = [("header", 0, 4), ("sigma", 4, 4), ("tau", 8, 4)]
for i, (f, off, sz) in enumerate(want):
    row = layout[i]
    assert row["field"] == f, row
    assert row["offset"] == off, row
    assert row["size"]   == sz,  row

rt = d["roundtrip"]
assert len(rt) == 4, rt
for row in rt:
    assert row["roundtrip_ok"] is True, row
    assert row["observed"] == row["expected"], row
assert d["rt_rows_ok"] == 4, d
assert d["rt_roundtrip_ok"] is True, d
assert d["rt_gate_verdict_ok"] is True, d

gate = d["gate"]
assert len(gate) == 3, gate
prev = -1
for row in gate:
    assert row["verdict"] > prev, (row, prev)
    prev = row["verdict"]
assert d["gate_rows_ok"] == 3, d
assert d["gate_order_ok"] is True, d
assert d["gate_pure_ok"]  is True, d

bench = d["bench"]
assert len(bench) == 3, bench
labels_want = ["encode_decode", "gate_allow", "gate_abstain"]
for i, row in enumerate(bench):
    assert row["label"] == labels_want[i], row
    assert row["iters"] >= 1_000_000, row
    assert row["min_ns"]  > 0.0, row
    assert row["mean_ns"] > 0.0, row
    assert row["mean_ns"] < 1_000_000.0, row
    assert row["min_ns"]  <= row["median_ns"] <= row["max_ns"], row
    assert row["under_budget"] is True, row
assert d["bench_rows_ok"] == 3, d
assert d["bench_budget_ok"] is True, d
assert d["bench_distribution_ok"] is True, d

sig = d["sigma_measurement"]
assert 0.0 <= sig <= 1.0, sig
assert abs(sig) < 1e-6, sig
assert d["chain_valid"] is True, d

print(f"v259: ns_per_call  encode_decode={bench[0]['mean_ns']:.1f}  "
      f"gate_allow={bench[1]['mean_ns']:.1f}  "
      f"gate_abstain={bench[2]['mean_ns']:.1f}")
PY

# Determinism: the structural fields (layout, gate, verdicts, hashes) must
# be byte-identical across runs.  We strip the per-run bench numbers then
# compare.
OUT2="$("$BIN")"
python3 - <<'PY' "$OUT" "$OUT2"
import json, sys
a = json.loads(sys.argv[1])
b = json.loads(sys.argv[2])
def strip(d):
    d = dict(d)
    d["bench"] = [
        {k: v for k, v in row.items() if k in ("label", "iters", "under_budget")}
        for row in d["bench"]
    ]
    return d
assert strip(a) == strip(b), "v259: structural determinism violated"
PY
