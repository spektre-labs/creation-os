#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Plot helper stub for v40 σ-threshold harness (reads JSON rows if present)."""
from __future__ import annotations

import glob
import json
import os
import sys


def main() -> int:
    pattern = os.path.join(os.path.dirname(__file__), "truthfulqa_*ch.json")
    paths = sorted(glob.glob(pattern))
    if not paths:
        print("plot_threshold.py: no benchmarks/v40/truthfulqa_*ch.json files found (stub).")
        return 0
    print(f"plot_threshold.py: found {len(paths)} json files (stub parser).")
    for p in paths[:3]:
        try:
            with open(p, "r", encoding="utf-8") as f:
                json.load(f)
        except OSError as e:
            print(p, e, file=sys.stderr)
            return 1
    print("plot_threshold.py: OK (JSON readable). Add plotting when metrics schema is stable.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
