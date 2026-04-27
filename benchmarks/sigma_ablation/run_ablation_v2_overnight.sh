#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# v2 overnight: same as run_ablation_overnight.sh but fails fast if SEU deps
# (sentence-transformers) are missing. Use from repo root or via Makefile.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "${REPO_ROOT}"

if ! python3 -c "from sentence_transformers import SentenceTransformer" 2>/dev/null; then
  echo "error: install sentence-transformers for SEU (see benchmarks/sigma_ablation/results/README.md)" >&2
  exit 2
fi
if ! python3 -c "from sklearn.metrics import roc_auc_score" 2>/dev/null; then
  echo "error: pip install scikit-learn" >&2
  exit 2
fi

exec bash "${SCRIPT_DIR}/run_ablation_overnight.sh"
