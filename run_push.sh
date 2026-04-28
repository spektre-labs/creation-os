#!/usr/bin/env bash
# Housekeeping: stage σ-lab paths, commit, tag, optional push.
# Review `git diff --cached` before pushing. Requires network auth for `git push`.
# Usage: bash run_push.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

echo "=== git status (short) ==="
git status --short

echo "=== staging σ-gate lab tree (excludes third_party/bitnet) ==="
git add .gitignore Makefile README.md REPRO_CHECKSUMS.txt TODO.md
git add python/cos/
git add benchmarks/sigma_gate_lsd/
git add benchmarks/sigma_gate_eval/
git add benchmarks/sigma_gate_scaling/
git add benchmarks/sigma_gate_v5/
git add benchmarks/sigma_probe_lsd/
git add configs/mcp/
git add scripts/cos_mcp_server.py
git add run_final_eval.sh run_holdout_pipeline.sh run_integration.sh run_lsd_full.sh \
  run_lsd_integration.sh run_lsd_v4.sh run_mcp_integration.sh run_multi_signal.sh \
  run_next_phase.sh run_ship.sh run_v5.sh 2>/dev/null || true
git add docs/MCP_SIGMA.md docs/sigma_gate_v4_comparison_table.md docs/sigma_gate_v4_publication_results.md \
  docs/reddit_ml_sigma_gate.md docs/reddit_ml_sigma_gate_v4.md docs/reddit_ml_post_v2.md 2>/dev/null || true
git add benchmarks/sigma_ablation/ benchmarks/sigma_probe/train_probe.py 2>/dev/null || true

echo "=== staged stat ==="
git diff --cached --stat

if git diff --cached --quiet; then
  echo "Nothing staged; exiting."
  exit 0
fi

git commit -m "σ-gate lab: LSD probe, eval harness, MCP stdio tools, and repro checksums"

TAG="v56"
if git rev-parse "$TAG" >/dev/null 2>&1; then
  echo "Tag $TAG already exists; skip tagging."
else
  git tag -a "$TAG" -m "σ-gate v4 lab: holdout + TriviaQA smoke + MCP stdio (see README, REPRO_CHECKSUMS.txt)"
fi

echo "=== log (last 5) ==="
git log --oneline -5

echo "To publish: git push origin HEAD && git push origin $TAG"
