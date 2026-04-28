#!/usr/bin/env python3
"""
σ-gate scaling **smoke**: document that LSD probes are **architecture- and width-locked**.

The bundled ``sigma_gate_lsd.pkl`` is trained on GPT-2 hidden trajectories; you cannot
score Llama/Gemma tensors without a new probe (or an explicit adapter + retrain).

This script prints environment guidance and optionally loads GPT-2 only.
Set ``SIGMA_SCALING_PROBE=1`` to attempt ``SigmaGate`` load on ``gpt2`` (cheap check).
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path


def _repo() -> Path:
    return Path(__file__).resolve().parents[2]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--hf-model",
        default="openai-community/gpt2",
        help="Only gpt2 is expected to match the shipped LSD pickle hidden layout.",
    )
    args = ap.parse_args()
    repo = _repo()
    print("sigma_gate_scaling / run_scaling.py")
    print("  LSD σ-gate pickle is tied to the probe manifest ``hf_model`` (typically gpt2).")
    print("  Larger checkpoints (Gemma, Phi, Llama) require re-running adapt_lsd on that model.")
    print("  See benchmarks/sigma_gate_lsd/adapt_lsd.py and docs/CLAIM_DISCIPLINE.md.")
    print()
    print("  Suggested HF ids (RAM approximate; not verified on this host):")
    for name, ram in [
        ("openai-community/gpt2", "~500 MB"),
        ("google/gemma-2-2b-it", "~4 GB"),
        ("microsoft/Phi-4-mini-instruct", "~8 GB"),
        ("meta-llama/Llama-3.2-3B-Instruct", "~6 GB (gated)"),
        ("google/gemma-2-9b-it", "~18 GB"),
    ]:
        print(f"    - {name:42s} {ram}")

    if os.environ.get("SIGMA_SCALING_PROBE", "") != "1":
        print("\nSet SIGMA_SCALING_PROBE=1 to run a GPT-2 SigmaGate smoke import.")
        return 0

    if "gpt2" not in str(args.hf_model).lower():
        print("SIGMA_SCALING_PROBE: only gpt2 is supported for pickle compatibility.", file=sys.stderr)
        return 2

    sys.path.insert(0, str(repo / "python"))
    p = repo / "benchmarks/sigma_gate_lsd/results_holdout/sigma_gate_lsd.pkl"
    if not p.is_file():
        p = repo / "benchmarks/sigma_gate_lsd/results_full/sigma_gate_lsd.pkl"
    if not p.is_file():
        print("No sigma_gate_lsd.pkl found.", file=sys.stderr)
        return 2

    from cos.sigma_gate import SigmaGate

    g = SigmaGate(str(p))
    g.close()
    print(f"\nSigmaGate load OK: {p}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
