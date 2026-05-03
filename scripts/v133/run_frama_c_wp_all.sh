#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Optional: tier-1 v259 Wp when Frama-C is installed; otherwise no-op success.
set -euo pipefail
cd "$(dirname "$0")/../.."
if ! command -v frama-c >/dev/null 2>&1; then
  echo "cos formal: frama-c not on PATH — skip Wp (see scripts/v259/run_frama_c_wp.sh)" >&2
  exit 0
fi
export COS_SKIP_INSTALL="${COS_SKIP_INSTALL:-1}"
exec bash scripts/v259/run_frama_c_wp.sh
