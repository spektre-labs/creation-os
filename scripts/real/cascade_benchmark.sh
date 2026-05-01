#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# Compare single-model σ-gate vs cascade vs cross-model σ on graded prompts.
# Requires: Ollama, ./cos-chat, benchmarks/graded/graded_prompts.csv
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
CSV="${1:-benchmarks/graded/graded_prompts.csv}"
N="${2:-20}"
OUT="${3:-benchmarks/graded/cascade_benchmark_out}"
mkdir -p "$(dirname "$OUT")"

if [[ ! -f "$CSV" ]]; then
  echo "Missing CSV: $CSV" >&2
  exit 2
fi

extract_prompts() {
  python3 -c "
import csv, sys
path = sys.argv[1]
n = int(sys.argv[2])
k = 0
with open(path, newline='', encoding='utf-8', errors='replace') as f:
    for row in csv.DictReader(f):
        p = (row.get('prompt') or '').strip()
        if not p:
            continue
        print(p)
        k += 1
        if k >= n:
            break
" "$CSV" "$N"
}

run_mode() {
  local tag="$1"
  shift
  local i=0
  while IFS= read -r prompt; do
    [[ -z "$prompt" ]] && continue
    i=$((i + 1))
    echo "--- $tag row $i ---"
    COS_ENGRAM_DISABLE=1 ./cos-chat --once --prompt "$prompt" "$@" 2>&1 \
      | tee -a "${OUT}_${tag}.log" || true
  done < <(extract_prompts)
}

echo "Writing logs under ${OUT}_*.log (N=$N)"
: >"${OUT}_A.log"
: >"${OUT}_B.log"
: >"${OUT}_C.log"
run_mode A --multi-sigma
run_mode B --cascade --multi-sigma
run_mode C --cross-model-sigma --multi-sigma
echo "Done. Post-process logs for AUROC/timing with your usual tooling."
