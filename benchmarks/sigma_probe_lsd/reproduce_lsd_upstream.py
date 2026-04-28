#!/usr/bin/env python3
"""
Run Sirraya LSD upstream pipeline from a Creation OS checkout.

Uses vendor code under external/lsd (clone Sirraya_LSD_Code there first).
Optional env: LSD_NUM_PAIRS, LSD_EPOCHS (defaults match lsd/main.py).
"""
from __future__ import annotations

import os
import sys
from pathlib import Path


def main() -> None:
    repo = Path(__file__).resolve().parents[2]
    lsd_root = repo / "external" / "lsd"
    if not lsd_root.is_dir():
        print("Missing external/lsd — clone https://github.com/sirraya-tech/Sirraya_LSD_Code.git", file=sys.stderr)
        sys.exit(2)
    os.chdir(lsd_root)
    sys.path.insert(0, str(lsd_root))
    from lsd.main import main as lsd_main  # noqa: PLC0415 — after chdir

    lsd_main()


if __name__ == "__main__":
    main()
