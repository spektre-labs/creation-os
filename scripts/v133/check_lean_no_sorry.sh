#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Strip Lean comments and fail on executable `sorry` (Measurement + v133 stack).
set -euo pipefail
cd "$(dirname "$0")/../.."
strip_check() {
  local f=$1
  python3 - "$f" <<'PY' || exit 1
import re, sys
src = open(sys.argv[1]).read()
src = re.sub(r'/-.*?-/', '', src, flags=re.DOTALL)
src = re.sub(r'--[^\n]*', '', src)
src = re.sub(r'`[^`]*`', '', src)
for i, line in enumerate(src.splitlines(), 1):
    if re.search(r'\bsorry\b', line):
        print(f"FAIL: {sys.argv[1]} line {i}: {line}", file=sys.stderr)
        sys.exit(1)
PY
}
strip_check hw/formal/v259/Measurement.lean
strip_check formal/lean/CreationOS/V133.lean
echo "cos formal --no-sorry: OK" >&2
exit 0
