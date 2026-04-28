#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Repo-root entry for σ-ablation v2 overnight driver (MCT + SEU + full matrix).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
exec bash "${ROOT}/benchmarks/sigma_ablation/run_ablation_v2_full.sh" "$@"
