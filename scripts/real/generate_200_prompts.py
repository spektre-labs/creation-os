#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Emit benchmarks/graded/graded_prompts.csv with 200 rows."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILDER = ROOT / "scripts" / "real" / "build_graded_prompts_200.py"


def main() -> int:
    if not BUILDER.is_file():
        print(f"generate_200_prompts: missing {BUILDER}", file=sys.stderr)
        return 1
    r = subprocess.run([sys.executable, str(BUILDER)], cwd=str(ROOT), check=False)
    return int(r.returncode)


if __name__ == "__main__":
    sys.exit(main())
