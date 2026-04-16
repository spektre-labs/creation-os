#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
Fail if docs/WHAT_IS_REAL.md is missing tier tags.

This is intentionally simple: it ensures the tier legend exists and that all
rows in the tables use one of the known one-letter tier tags.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ALLOWED = {"M", "T", "P", "E", "N", "R"}


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    p = root / "docs" / "WHAT_IS_REAL.md"
    s = p.read_text(encoding="utf-8")

    if "## Tier legend" not in s and "Tier legend" not in s:
        print("WHAT_IS_REAL tier check: missing tier legend section", file=sys.stderr)
        return 2

    # Match markdown table rows where the last column is **X** or X.
    bad = []
    for ln, line in enumerate(s.splitlines(), start=1):
        if "|" not in line:
            continue
        if re.match(r"^\s*\|\s*-+\s*\|", line):
            continue  # separator
        # Candidate: ends with | **X** |
        m = re.search(r"\|\s*(\*\*)?([A-Z])(\*\*)?\s*\|\s*$", line)
        if not m:
            continue
        tier = m.group(2)
        if tier not in ALLOWED:
            bad.append((ln, tier, line.strip()))

    if bad:
        for ln, tier, line in bad[:20]:
            print(f"WHAT_IS_REAL tier check: bad tier '{tier}' at line {ln}: {line}", file=sys.stderr)
        return 2

    print("WHAT_IS_REAL tier check: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

