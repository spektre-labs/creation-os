#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
#
# v111.2 pre-registered adaptive σ matrix smoke test.
#
# Invariants:
#   * PREREGISTRATION_ADAPTIVE.lock.json exists.
#   * adaptive_signals.py SHA-256 matches the lock file (no drift).
#   * Bonferroni N == 12.
#   * frontier_matrix_prereg.{md,json} is emitted with
#     preregistered=true.
#   * conformal_tau.json is emitted with alpha==0.05.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

PY="${COS_V111_PY:-.venv-bitnet/bin/python}"
if [ ! -x "$PY" ]; then
    PY="$(command -v python3)"
fi

LOCK="benchmarks/v111/PREREGISTRATION_ADAPTIVE.lock.json"
OUT_MD="benchmarks/v111/results/frontier_matrix_prereg.md"
OUT_JSON="benchmarks/v111/results/frontier_matrix_prereg.json"
OUT_CONF="benchmarks/v111/results/conformal_tau.json"

test -s "$LOCK" || { echo "FAIL: $LOCK missing. Run: .venv-bitnet/bin/python benchmarks/v111/preregister_adaptive.py --lock" >&2; exit 1; }

echo "v111.2 pre-reg: re-running analysis against frozen lock ..."
"$PY" benchmarks/v111/preregister_adaptive.py --analyse >/dev/null

test -s "$OUT_MD"   || { echo "FAIL: $OUT_MD missing"   >&2; exit 1; }
test -s "$OUT_JSON" || { echo "FAIL: $OUT_JSON missing" >&2; exit 1; }
test -s "$OUT_CONF" || { echo "FAIL: $OUT_CONF missing" >&2; exit 1; }

"$PY" - "$OUT_JSON" "$OUT_CONF" "$LOCK" <<'PY'
import json, sys, hashlib, os
path_json, path_conf, path_lock = sys.argv[1], sys.argv[2], sys.argv[3]
with open(path_json) as f:  m = json.load(f)
with open(path_conf) as f:  c = json.load(f)
with open(path_lock) as f:  l = json.load(f)

assert m.get("preregistered") is True, "prereg JSON not labelled preregistered"
assert m.get("bonferroni_N") == 12, f"Bonferroni N drift: {m.get('bonferroni_N')}"
assert abs(m.get("alpha_fw", 0.0) - 0.05/12) < 1e-6, "α_fw drift"

sig_path = os.path.join(os.path.dirname(path_lock), "adaptive_signals.py")
with open(sig_path, "rb") as f:
    sha_now = hashlib.sha256(f.read()).hexdigest()
assert sha_now == l["signals_sha256"], (
    "adaptive_signals.py drifted from lock file. "
    "Revert it, or create a NEW pre-registration."
)

assert c.get("alpha") == 0.05, f"conformal α drift: {c.get('alpha')}"
assert c.get("preregistered") is True, "conformal JSON not preregistered"

# Check at least one test-split winner OR report failure honestly.
winners = []
for t in m.get("tasks", []):
    for r in t.get("rows", []):
        if r.get("bonferroni_significant"):
            winners.append((t["task"], r["signal"]))

print(f"v111.2 pre-reg OK — lock={l['signals_sha256'][:12]}..., "
      f"N={m['bonferroni_N']}, any_winner={m.get('any_winner')}, "
      f"winners={winners}")
PY

echo "v111.2 pre-registered adaptive matrix + conformal τ OK"
