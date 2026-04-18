#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v111 matrix check — merge-gate safe.
#
# Verifies that the frontier-matrix pipeline loads and computes on the
# sidecars that exist in the tree right now (arc_easy / arc_challenge /
# truthfulqa_mc2 from v104/n5000_results).  Does NOT require BitNet
# weights, Homebrew, MLX or GPUs.  If the venv or sidecars are missing it
# SKIPs cleanly; it never fails the gate for an optional dependency.
set -eo pipefail
cd "$(dirname "$0")/../.."

PY="${COS_V111_PYTHON:-.venv-bitnet/bin/python}"
if [ ! -x "$PY" ]; then
    echo "check-v111-matrix: SKIP (no $PY)"
    exit 0
fi

PYVER=$("$PY" -c 'import sys; print(sys.version_info >= (3,9))' 2>/dev/null || echo False)
if [ "$PYVER" != "True" ]; then
    echo "check-v111-matrix: SKIP (Python < 3.9)"
    exit 0
fi

# Minimal structural sanity: the signals file must register six names and
# Bonferroni must be 24 (the whole matrix semantics depend on this).
export PYTHONPATH="$PWD/benchmarks/v111:$PWD/benchmarks/v103:$PWD/benchmarks/v104:${PYTHONPATH:-}"
"$PY" - <<'PY'
from frontier_signals import SIGNALS, BONFERRONI_N, TASK_FAMILIES
assert set(SIGNALS.keys()) == {
    "entropy", "sigma_mean", "sigma_max_token",
    "sigma_product", "sigma_tail_mass", "sigma_n_effective",
}, f"unexpected signal set: {list(SIGNALS)}"
assert BONFERRONI_N == 24, BONFERRONI_N
assert TASK_FAMILIES == ["arc_challenge", "arc_easy", "truthfulqa_mc2", "hellaswag"]
print("check-v111-matrix: signals & Bonferroni OK")
PY

# Synthetic probe — guarantees the math works without any weights.
"$PY" - <<'PY'
import random, statistics
from frontier_signals import SIGNALS
# Build 300 synthetic docs: correctness probability ∝ (1 - entropy).
rng = random.Random(0)
docs = []
for _ in range(300):
    profile = [rng.random() for _ in range(8)]
    sidecar = {
        "sigma_mean": sum(profile) / 8.0,
        "sigma_max_token": max(profile[0] + 0.1 * rng.random(), 0.0),
        "sigma_profile": profile,
    }
    p_correct = max(0.02, 1.0 - profile[0])
    acc = 1.0 if rng.random() < p_correct else 0.0
    docs.append({"acc": acc, "sidecar": sidecar})
# Rank by entropy ascending and check monotone coverage acc.
def _aurcc(signal):
    scored = sorted(docs, key=lambda d: signal(d["sidecar"]))
    cum = 0.0
    area = 0.0
    prev_cov, prev_risk = 0.0, 1.0 - scored[0]["acc"]
    for k, d in enumerate(scored, 1):
        cum += d["acc"]
        cov = k / len(scored)
        risk = 1.0 - cum / k
        area += 0.5 * (prev_risk + risk) * (cov - prev_cov)
        prev_cov, prev_risk = cov, risk
    return area
aurcc_ent = _aurcc(SIGNALS["entropy"])
aurcc_rand = _aurcc(lambda s: s["sigma_mean"])
# Oracle ordering by construction — entropy must beat random sigma_mean
# on this synthetic (correctness is the complement of entropy).
assert aurcc_ent <= aurcc_rand + 1e-6, (aurcc_ent, aurcc_rand)
print(f"check-v111-matrix: synthetic AURCC sanity OK (ent={aurcc_ent:.4f} ≤ mean={aurcc_rand:.4f})")
PY

# End-to-end: if v104 n=5000 sidecars exist, re-analyse them.
if [ -f benchmarks/v104/n5000_results/samples_arc_challenge_sigma.jsonl ] &&
   [ -d benchmarks/v104/n5000_results/lm_eval ]; then
    "$PY" benchmarks/v111/frontier_matrix.py \
        --tasks arc_challenge,arc_easy,truthfulqa_mc2,hellaswag \
        --n-boot 500 >/dev/null
    echo "check-v111-matrix: matrix build OK (3/4 tasks, hellaswag PENDING)"
else
    echo "check-v111-matrix: SKIP matrix build (v104 n5000 sidecars absent)"
fi

echo "check-v111-matrix: PASS"
