#!/usr/bin/env bash
# Master benchmark driver: graded → metrics → optional TruthfulQA → σ-separation
# → optional evolve → DEFINITIVE_RESULTS.md.
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Prefer a non–iCloud clone (e.g. ~/Projects/creation-os-kernel); see benchmarks/FULL_REPORT.md.
#
# Environment (all optional):
#   METACOG_RUN_GRADED=1          Run scripts/real/graded_benchmark.sh (50 prompts).
#   METACOG_RUN_TRUTHFULQA=0      Set to 1 for TruthfulQA MC1 (default N=100; long).
#   TRUTHFULQA_N                  Passed through to truthfulqa_sigma.sh.
#   METACOG_RUN_SIGMA_SEP=1       Run scripts/real/sigma_separation_pipeline_coschat.sh.
#   METACOG_RUN_EVOLVE=0          Set to 1 for omega_evolve_final.sh (network + time).
#   COS_EVOLVE_FINAL_CLEAN=1      When METACOG_RUN_EVOLVE=1, removes ~/.cos/engram_*.db first.
#   DEFINITIVE_HW                 Hardware line for DEFINITIVE_RESULTS.md.
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

case "$ROOT" in
  *Desktop*|*iCloud*|*icloud*)
    echo "warning: Desktop/iCloud paths can stall dyld/git; prefer ~/Projects/creation-os-kernel." >&2
    ;;
esac

export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-11434}"
export COS_BITNET_CHAT_MODEL="${COS_BITNET_CHAT_MODEL:-gemma3:4b}"
export COS_ENGRAM_DISABLE="${COS_ENGRAM_DISABLE:-1}"

METACOG_RUN_GRADED="${METACOG_RUN_GRADED:-1}"
METACOG_RUN_TRUTHFULQA="${METACOG_RUN_TRUTHFULQA:-0}"
METACOG_RUN_SIGMA_SEP="${METACOG_RUN_SIGMA_SEP:-1}"
METACOG_RUN_EVOLVE="${METACOG_RUN_EVOLVE:-0}"

if [[ ! -x ./cos ]]; then
  echo "error: ./cos missing — run: make cos" >&2
  exit 1
fi

if [[ "$METACOG_RUN_GRADED" == "1" ]]; then
  echo "=== Graded benchmark (50 prompts) ==="
  bash scripts/real/graded_benchmark.sh
fi

if [[ -f benchmarks/graded/graded_results.csv ]]; then
  echo "=== Metacognitive profile (stdout) ==="
  python3 scripts/real/metacognitive_profile.py || true
fi

if [[ "$METACOG_RUN_TRUTHFULQA" == "1" ]]; then
  echo "=== TruthfulQA (MC1) ==="
  TRUTHFULQA_N="${TRUTHFULQA_N:-100}" bash scripts/real/truthfulqa_sigma.sh || true
fi

if [[ "$METACOG_RUN_SIGMA_SEP" == "1" ]]; then
  echo "=== σ-separation pipeline (15 prompts) ==="
  bash scripts/real/sigma_separation_pipeline_coschat.sh || true
fi

if [[ "$METACOG_RUN_EVOLVE" == "1" ]]; then
  echo "=== Evolve campaign (removing engram DBs, then omega_evolve_final) ==="
  rm -f "${HOME}/.cos/engram_episodes.db" "${HOME}/.cos/engram_semantic.db" 2>/dev/null || true
  export COS_EVOLVE_FINAL_CLEAN="${COS_EVOLVE_FINAL_CLEAN:-1}"
  bash scripts/real/omega_evolve_final.sh || true
fi

echo "=== DEFINITIVE_RESULTS.md ==="
python3 scripts/real/generate_definitive_results.py

echo "Done. See benchmarks/DEFINITIVE_RESULTS.md"
