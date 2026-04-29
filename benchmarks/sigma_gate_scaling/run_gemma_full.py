#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Gemma **full** eval orchestrator (default ``n=30`` holdout rows, optional multi-signal).

This is a thin wrapper around ``run_gemma_eval.py`` so nightly / docs can point at one entry
without duplicating load logic. **LSD probe training** on Gemma hidden states (dim 2304) is
**not** implemented here: use the GPT-2-oriented ``benchmarks/sigma_gate_lsd/`` collectors and
swap the backbone in a dedicated branch when you have a frozen Gemma hidden-state manifest.

Requires ``HF_TOKEN`` or ``HUGGING_FACE_HUB_TOKEN`` (never commit tokens).

See ``docs/CLAIM_DISCIPLINE.md`` for measured vs target AUROC language.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--limit", type=int, default=30, help="holdout rows (default 30)")
    ap.add_argument("--signals", choices=("hide", "icr", "both"), default="both")
    ap.add_argument("--hide-layers", default="4,5,6,7,8,9,10", help="comma block indices; empty = single middle layer")
    ap.add_argument("--cpu", action="store_true", help="forward to run_gemma_eval --cpu")
    ap.add_argument("--max-new-tokens", type=int, default=100)
    ap.add_argument("--no-prism", action="store_true")
    args = ap.parse_args()

    script = _repo_root() / "benchmarks" / "sigma_gate_scaling" / "run_gemma_eval.py"
    cmd = [
        sys.executable,
        str(script),
        "--limit",
        str(int(args.limit)),
        "--signals",
        str(args.signals),
        "--max-new-tokens",
        str(int(args.max_new_tokens)),
    ]
    hl = (args.hide_layers or "").strip()
    if hl:
        cmd.extend(["--hide-layers", hl])
    if bool(args.cpu):
        cmd.append("--cpu")
    if bool(args.no_prism):
        cmd.append("--no-prism")

    return int(subprocess.call(cmd, cwd=str(_repo_root())))


if __name__ == "__main__":
    raise SystemExit(main())
