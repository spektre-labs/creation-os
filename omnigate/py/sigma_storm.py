#!/usr/bin/env python3
"""ASCII lattice + merge-gate — stdlib only."""
from __future__ import annotations

import os
import subprocess
import sys


def storm() -> None:
    σ = "σ"
    rows = 6
    for i in range(rows):
        pad = " " * (rows - i)
        line = pad + (σ + " ") * (2 * i + 1)
        print(line, file=sys.stderr)


def main() -> int:
    storm()
    root = os.environ.get("CREATION_OS_ROOT", ".")
    r = subprocess.run(["make", "merge-gate"], cwd=root)
    return int(r.returncode)


if __name__ == "__main__":
    raise SystemExit(main())
