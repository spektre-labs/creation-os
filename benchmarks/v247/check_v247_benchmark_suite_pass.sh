#!/usr/bin/env bash
#
# v247 σ-Benchmark-Suite — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * exactly 4 categories in canonical order
#     (correctness, performance, cognitive, comparative)
#   * correctness: 4 tests in order (unit, integration,
#     e2e, regression) all pass
#   * performance: p50 <= p95 <= p99, throughput_rps > 0,
#     sigma_overhead < tau_overhead (0.05), overhead_ok
#   * cognitive: consistency_stable == consistency_trials,
#     adversarial_pass == adversarial_total,
#     accuracy_answered and abstention_rate in [0,1]
#   * comparative: two rows named creation_os_vs_raw_llama
#     and creation_os_vs_openai_api (in order)
#   * CI targets: exactly {test, bench, verify} in order
#   * sigma_suite in [0,1] AND == 0.0 (100% pass)
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v247_benchmark_suite"
[ -x "$BIN" ] || { echo "v247: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v247", d
assert d["chain_valid"] is True, d

assert d["categories"] == ["correctness","performance",
                            "cognitive","comparative"], d
assert d["n_categories"] == 4, d

cor = [c["name"] for c in d["correctness"]]
assert cor == ["unit","integration","e2e","regression"], cor
for c in d["correctness"]:
    assert c["pass"] is True, c

p = d["performance"]
assert p["latency_p50_ms"] <= p["latency_p95_ms"] + 1e-6, p
assert p["latency_p95_ms"] <= p["latency_p99_ms"] + 1e-6, p
assert p["throughput_rps"] > 0, p
assert abs(p["tau_overhead"] - 0.05) < 1e-6, p
assert 0.0 <= p["sigma_overhead"] <  p["tau_overhead"], p
assert p["overhead_ok"] is True, p

cg = d["cognitive"]
assert 0.0 <= cg["accuracy_answered"] <= 1.0, cg
assert 0.0 <= cg["abstention_rate"]   <= 1.0, cg
assert cg["consistency_trials"] > 0, cg
assert cg["consistency_stable"] == cg["consistency_trials"], cg
assert cg["adversarial_total"] > 0, cg
assert cg["adversarial_pass"] == cg["adversarial_total"], cg

comp = [c["name"] for c in d["comparative"]]
assert comp == ["creation_os_vs_raw_llama",
                 "creation_os_vs_openai_api"], comp

assert d["ci_targets"] == ["test","bench","verify"], d

assert d["n_passing_tests"] == d["n_total_tests"], d
assert 0.0 <= d["sigma_suite"] <= 1.0, d
assert d["sigma_suite"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v247: non-deterministic" >&2; exit 1; }
