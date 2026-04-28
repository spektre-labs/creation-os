#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# σ-ablation v2 driver: optional dependency install, embedding preload, then
# the same phases as run_ablation_overnight.sh (full matrix + analyze --plots + optional git).
#
# From repo root:
#   chmod +x benchmarks/sigma_ablation/run_ablation_v2_full.sh
#   nohup bash benchmarks/sigma_ablation/run_ablation_v2_full.sh >> benchmarks/sigma_ablation/logs/nohup_v2.log 2>&1 &
#
# Environment:
#   SIGMA_ABLATION_V2_AUTO_PIP=1  — pip install sentence-transformers scikit-learn (use with care)
#   SIGMA_ABLATION_GIT_PUSH / SIGMA_ABLATION_GIT_COMMIT — same as run_ablation_overnight.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "${REPO_ROOT}"

# Detached launches (setsid/nohup) may start with a minimal PATH; match the
# interpreter that has sentence-transformers after `pip install …`.
export PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:${PATH}"
if [[ -z "${PYTHON:-}" ]]; then
  PYTHON="$(command -v python3 2>/dev/null || true)"
fi
if [[ -z "${PYTHON}" ]]; then
  PYTHON="python3"
fi

if [[ "${SIGMA_ABLATION_V2_AUTO_PIP:-0}" == "1" ]]; then
  echo "SIGMA_ABLATION_V2_AUTO_PIP=1 — installing Python deps..."
  "${PYTHON}" -m pip install --user sentence-transformers scikit-learn 2>/dev/null || true
fi

echo "=== Pre-load embedding model (SEU) ==="
"${PYTHON}" -c "
from sentence_transformers import SentenceTransformer
m = SentenceTransformer('sentence-transformers/all-MiniLM-L6-v2')
print('embedding dim:', m.get_sentence_embedding_dimension())
" || {
  echo "warning: sentence-transformers preload failed (SEU rows may be skipped)" >&2
}

exec bash "${SCRIPT_DIR}/run_ablation_overnight.sh"
