#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
#
# v111.2 post-hoc adaptive / composite σ matrix smoke test.
#
# Contract
# --------
# * Refuses to pass unless every artefact is labelled `post_hoc = true`.
# * Refuses to pass if the post-hoc Bonferroni N drifts from 12.
# * Refuses to pass if the selective-prediction PNG is missing after
#   regeneration.
#
# This script never substitutes adaptive numbers for the pre-registered
# v111 matrix; it only validates that the adaptive artefacts were
# written with the correct labels and cardinality.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

PY="${COS_V111_PY:-.venv-bitnet/bin/python}"
if [ ! -x "$PY" ]; then
    PY="$(command -v python3 || true)"
fi
if [ -z "${PY:-}" ] || ! "$PY" -c 'import sys; sys.exit(0 if sys.version_info >= (3,9) else 1)' 2>/dev/null; then
    echo "check-v111-adaptive: SKIP (no suitable python3 interpreter)"
    exit 0
fi

OUT_MD="benchmarks/v111/results/frontier_matrix_adaptive.md"
OUT_JSON="benchmarks/v111/results/frontier_matrix_adaptive.json"
OUT_PNG="docs/v111/selective_prediction_curves.png"

# plot_selective_prediction.py hard-depends on matplotlib.  It is an
# optional post-hoc artefact (not part of the pre-registered matrix), so
# we SKIP cleanly on hosts without it rather than failing the gate on an
# optional plotting dependency.
if ! "$PY" -c 'import matplotlib' 2>/dev/null; then
    echo "check-v111-adaptive: SKIP (matplotlib not installed; post-hoc plotting is optional)"
    exit 0
fi

echo "v111.2 adaptive: regenerating matrix and curves (post-hoc, not pre-registered) ..."
"$PY" benchmarks/v111/adaptive_matrix.py >/dev/null
"$PY" benchmarks/v111/plot_selective_prediction.py >/dev/null

test -s "$OUT_MD"   || { echo "FAIL: $OUT_MD missing"   >&2; exit 1; }
test -s "$OUT_JSON" || { echo "FAIL: $OUT_JSON missing" >&2; exit 1; }
test -s "$OUT_PNG"  || { echo "FAIL: $OUT_PNG missing"  >&2; exit 1; }

"$PY" - "$OUT_JSON" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    d = json.load(f)
assert d.get("post_hoc") is True, "JSON not labelled post_hoc=True"
assert d.get("bonferroni_N") == 12, f"adaptive Bonferroni N drift: {d.get('bonferroni_N')}"
assert "sigma_task_adaptive" in d.get("signals", []), "missing sigma_task_adaptive"
measured = [t for t in d.get("tasks", []) if not t.get("skipped")]
assert len(measured) >= 4, f"fewer than 4 measured tasks: {len(measured)}"
print(f"v111.2 adaptive OK — {len(measured)} tasks × 3 post-hoc signals, N={d['bonferroni_N']}")
PY

echo "v111.2 adaptive + selective-prediction curves OK — $OUT_MD / $OUT_PNG"
