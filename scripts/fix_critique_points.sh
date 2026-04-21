#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Surgical fixer for critique follow-ups.
# Intentionally small: no downloads, no rewrites, no side effects beyond archiving
# exact-duplicate GDA files (if present).
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

echo "fix_critique_points: archive GDA duplicates (exact matches only)"
python3 tools/archive_gda_duplicates.py

echo "fix_critique_points: verify WHAT_IS_REAL tier tags"
python3 tools/check_what_is_real_tiers.py

echo "fix_critique_points: OK"

