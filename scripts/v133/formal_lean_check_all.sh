#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail
cd "$(dirname "$0")/../.."
if ! command -v lake >/dev/null 2>&1 || ! command -v lean >/dev/null 2>&1; then
  echo "cos formal: lake/lean not on PATH — skip Lean typecheck (structural CI still runs)" >&2
  exit 0
fi
( cd hw/formal/v259 && lake build )
( cd formal/lean && lake build )
echo "cos formal --lean --check-all: OK (v259 + CreationOS.V133)" >&2
exit 0
