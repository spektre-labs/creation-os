#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v103 check — SKIP-aware smoke, never fails merge-gate.
#
# If .venv-bitnet is missing or Python < 3.10, SKIP (like check-v102).
# Otherwise import the v103 backend module to confirm registration, and
# run compute_rc_metrics.py in synthetic mode (no real model call).
set -eo pipefail
cd "$(dirname "$0")/../.."

PY="${COS_V103_PYTHON:-.venv-bitnet/bin/python}"
if [ ! -x "$PY" ]; then
    echo "check-v103: SKIP (no $PY — install .venv-bitnet or set COS_V103_PYTHON)"
    exit 0
fi

PYVER=$("$PY" -c 'import sys; print(sys.version_info >= (3,10))' 2>/dev/null || echo False)
if [ "$PYVER" != "True" ]; then
    echo "check-v103: SKIP (Python < 3.10; lm-eval needs 3.10+)"
    exit 0
fi

# Include the editable lm-eval source dir directly in PYTHONPATH to avoid
# Python 3.12's "Skipping hidden .pth file" behavior (triggered by macOS
# provenance xattr on files under a project folder).  If lm-eval is installed
# normally (non-editable) this branch is a no-op.
LMEVAL_SRC="$PWD/third_party/lm-eval"
if [ -f "$LMEVAL_SRC/lm_eval/__init__.py" ]; then
    export PYTHONPATH="$LMEVAL_SRC:$PWD/benchmarks/v103:${PYTHONPATH:-}"
else
    export PYTHONPATH="$PWD/benchmarks/v103:${PYTHONPATH:-}"
fi

if ! "$PY" -c 'import lm_eval' >/dev/null 2>&1; then
    echo "check-v103: SKIP (lm_eval not importable in $PY)"
    exit 0
fi

if ! "$PY" -c 'import creation_os_v103_backend; print("ok")' >/dev/null 2>&1; then
    echo "check-v103: FAIL (backend import error)" >&2
    "$PY" -c 'import creation_os_v103_backend' 2>&1 | head -5
    exit 1
fi

# Synthetic sanity test on compute_rc_metrics: feed fake docs, check AURCC of
# an oracle signal is close to 0 and random signal is close to 1 - acc.
"$PY" - <<'PY'
import sys, random, statistics
sys.path.insert(0, "benchmarks/v103")
from compute_rc_metrics import _rc_curve, _aurcc, _coverage_at_acc

rng = random.Random(0)
N = 500
docs = []
for i in range(N):
    correct = rng.random() < 0.76
    # σ ~ low if correct, high if wrong (perfect oracle)
    sig_oracle = rng.uniform(0.0, 0.3) if correct else rng.uniform(0.7, 1.0)
    # random noise signal
    docs.append({
        "acc": 1.0 if correct else 0.0,
        "sigma_mean":       sig_oracle,
        "sigma_max_token":  sig_oracle,
        "sigma_profile":    [sig_oracle] * 8,
    })

curve_oracle = _rc_curve(docs, "sigma_mean")
curve_random = _rc_curve(docs, "random")
a_oracle = _aurcc(curve_oracle)
a_random = _aurcc(curve_random)
assert a_oracle < 0.10, f"oracle AURCC unexpectedly high: {a_oracle:.3f}"
assert a_random > 0.15, f"random AURCC unexpectedly low:  {a_random:.3f}"
c95_oracle = _coverage_at_acc(curve_oracle, 0.95)
assert c95_oracle >= 0.6, f"oracle Cov@0.95 too low: {c95_oracle:.3f}"
print(f"check-v103 synth: oracle AURCC={a_oracle:.4f}  random AURCC={a_random:.4f}  Cov@0.95(oracle)={c95_oracle:.3f}")
PY

echo "check-v103: OK (v103 backend imports; RC-metric synthetic oracle/random sanity)"
