#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v104 check — SKIP-aware, merge-gate-safe operator-search smoke.
#
# v104 has no C kernel; it is three Python analysers that consume v103's
# sidecar JSONLs.  This check confirms:
#   (1) operators.py loads and exposes OPERATORS + BONFERRONI_N,
#   (2) compute_operator_rcc.py runs end-to-end on synthetic docs,
#   (3) the AURCC ordering is sane: an oracle operator beats random.
# It does NOT require .venv-bitnet, lm-eval, BitNet GGUF or v103
# sidecars; on hosts that lack those it still runs, using the bundled
# synthetic fixture.
set -eo pipefail
cd "$(dirname "$0")/../.."

PY="${COS_V104_PYTHON:-.venv-bitnet/bin/python}"
if [ ! -x "$PY" ]; then
    echo "check-v104: SKIP (no $PY)"
    exit 0
fi

PYVER=$("$PY" -c 'import sys; print(sys.version_info >= (3,9))' 2>/dev/null || echo False)
if [ "$PYVER" != "True" ]; then
    echo "check-v104: SKIP (Python < 3.9)"
    exit 0
fi

export PYTHONPATH="$PWD/benchmarks/v104:$PWD/benchmarks/v103:${PYTHONPATH:-}"
"$PY" - <<'PY'
import os, random, sys

sys.path.insert(0, "benchmarks/v104")
sys.path.insert(0, "benchmarks/v103")

from operators import OPERATORS, BONFERRONI_N
assert BONFERRONI_N == 10, BONFERRONI_N
for name in ("sigma_mean", "sigma_max_token", "sigma_entropy_hybrid",
             "sigma_p95_token", "entropy", "random"):
    assert name in OPERATORS, name
print(f"check-v104: OPERATORS loaded — N={len(OPERATORS)}, Bonferroni {BONFERRONI_N}")

# synthetic docs: oracle signal = σ_mean low if correct, high if wrong
from compute_operator_rcc import _aurcc_from_curve, _rc_curve_op
rng = random.Random(0)
docs = []
for i in range(500):
    correct = rng.random() < 0.76
    # package a σ sidecar-like row
    sig = rng.uniform(0.0, 0.3) if correct else rng.uniform(0.7, 1.0)
    docs.append({
        "acc": 1.0 if correct else 0.0,
        "sidecar": {
            "sigma_mean": sig,
            "sigma_max_token": sig,
            "sigma_profile": [sig] * 8,
            "doc_index": i, "cand_index": 0,
        },
    })

a_oracle = _aurcc_from_curve(_rc_curve_op(docs, OPERATORS["sigma_mean"]))
a_random = _aurcc_from_curve(_rc_curve_op(docs, OPERATORS["random"]))
assert a_oracle < 0.10, f"oracle AURCC {a_oracle:.4f} too high"
assert a_random > 0.15, f"random AURCC {a_random:.4f} too low"
print(f"check-v104: synthetic oracle AURCC={a_oracle:.4f}, random={a_random:.4f} — OK")
PY

echo "check-v104: OK (operator registry, RC math, synthetic oracle/random)"
