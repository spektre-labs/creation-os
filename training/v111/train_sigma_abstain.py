# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v111.3 — MLX LoRA fine-tune for σ-supervised abstention.

Wraps `mlx_lm.lora` (the Apple MLX reference LoRA trainer) with Creation
OS specific defaults:

  * base model:  mlx-community/TinyLlama-1.1B-Chat-v1.0-4bit  (≈ 2 GB at
                 4-bit; runs comfortably on an 8 GB M3)
  * data:        training/v111/data/sigma_abstain.jsonl        (extracted
                 by `extract_sigma_training_data.py`; split 90/5/5)
  * output:      models/v111/sigma_abstain_lora/               (safetensors
                 adapter + config; loadable by mlx_lm.generate --adapter-path)

The training objective is standard causal-LM next-token prediction on the
JSONL `prompt + completion` concatenation — same as mlx_lm.lora defaults.
The σ-supervision is *in the data* (the completion is either "I don't
know." for high-σ prompts or the model's own confident answer for
low-σ prompts).  No custom loss, no custom optimizer — this is the
simplest possible fine-tune that nevertheless grounds abstention in the
model's measured uncertainty.

CLI examples
------------
    # full smoke (≈ 1 h on M3, 50 steps)
    .venv-bitnet/bin/python training/v111/train_sigma_abstain.py --iters 50

    # single-step dry-run  (CI smoke, ~60 s)
    .venv-bitnet/bin/python training/v111/train_sigma_abstain.py --iters 1

    # full SFT run         (≈ 12 h on M3, 2 epochs)
    .venv-bitnet/bin/python training/v111/train_sigma_abstain.py --iters 600
"""

from __future__ import annotations

import argparse
import json
import os
import random
import shutil
import subprocess
import sys
from typing import List, Tuple

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))


def _split_data(src_jsonl: str, out_dir: str,
                train_ratio: float = 0.90,
                valid_ratio: float = 0.05,
                seed: int = 0) -> Tuple[int, int, int]:
    with open(src_jsonl, "r", encoding="utf-8") as fh:
        rows: List[str] = [ln.strip() for ln in fh if ln.strip()]
    rng = random.Random(seed)
    rng.shuffle(rows)
    n_total = len(rows)
    n_train = max(1, int(train_ratio * n_total))
    n_valid = max(1, int(valid_ratio * n_total))
    n_test  = max(1, n_total - n_train - n_valid)
    # Guarantee at least 1 row per split (MLX LoRA requires all three).
    if n_train + n_valid + n_test > n_total:
        n_train = n_total - 2
        n_valid = 1
        n_test  = 1
    os.makedirs(out_dir, exist_ok=True)
    def dump(path: str, lines: List[str]) -> None:
        with open(path, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines) + ("\n" if lines else ""))
    dump(os.path.join(out_dir, "train.jsonl"), rows[:n_train])
    dump(os.path.join(out_dir, "valid.jsonl"), rows[n_train:n_train + n_valid])
    dump(os.path.join(out_dir, "test.jsonl"),  rows[n_train + n_valid:n_train + n_valid + n_test])
    return n_train, n_valid, n_test


def _ensure_mlx() -> None:
    try:
        import mlx.core  # noqa: F401
        import mlx_lm    # noqa: F401
    except ImportError as e:
        print(f"v111.3: MLX missing — `.venv-bitnet/bin/pip install mlx mlx-lm`  ({e})",
              file=sys.stderr)
        raise SystemExit(3)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default=os.path.join(REPO, "training/v111/data/sigma_abstain.jsonl"))
    ap.add_argument("--split-dir", default=os.path.join(REPO, "training/v111/data/splits"))
    ap.add_argument("--base-model",
                    default="mlx-community/TinyLlama-1.1B-Chat-v1.0-4bit",
                    help="HF-style repo or local path to a MLX-format model")
    ap.add_argument("--adapter-path",
                    default=os.path.join(REPO, "models/v111/sigma_abstain_lora"))
    ap.add_argument("--iters", type=int, default=50)
    ap.add_argument("--batch-size", type=int, default=1)
    ap.add_argument("--learning-rate", type=float, default=1e-4)
    ap.add_argument("--num-layers", type=int, default=8,
                    help="LoRA target layers (last N of the stack)")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--steps-per-eval", type=int, default=25)
    ap.add_argument("--save-every", type=int, default=25)
    ap.add_argument("--dry-run", action="store_true",
                    help="print the mlx_lm.lora command line and exit")
    args = ap.parse_args()

    if not os.path.isfile(args.data):
        print(f"v111.3: data not found: {args.data}", file=sys.stderr)
        print("         run training/v111/extract_sigma_training_data.py first",
              file=sys.stderr)
        return 2

    n_train, n_valid, n_test = _split_data(args.data, args.split_dir, seed=args.seed)
    print(f"v111.3: data split (seed={args.seed}) — "
          f"train={n_train}  valid={n_valid}  test={n_test}")

    os.makedirs(args.adapter_path, exist_ok=True)

    cmd = [
        sys.executable, "-m", "mlx_lm.lora",
        "--model", args.base_model,
        "--data", args.split_dir,
        "--adapter-path", args.adapter_path,
        "--train",
        "--iters", str(args.iters),
        "--batch-size", str(args.batch_size),
        "--learning-rate", f"{args.learning_rate}",
        "--num-layers", str(args.num_layers),
        "--seed", str(args.seed),
        "--steps-per-eval", str(args.steps_per_eval),
        "--save-every", str(args.save_every),
    ]
    print("v111.3: mlx_lm.lora command:")
    print("  " + " ".join(cmd))
    if args.dry_run:
        return 0

    _ensure_mlx()
    try:
        rc = subprocess.call(cmd, cwd=REPO)
    except FileNotFoundError as e:
        print(f"v111.3: cannot run mlx_lm.lora: {e}", file=sys.stderr)
        return 3
    if rc != 0:
        print(f"v111.3: mlx_lm.lora exited {rc}", file=sys.stderr)
        return rc

    # record a provenance manifest alongside the adapter
    manifest = {
        "tool": "mlx_lm.lora",
        "base_model": args.base_model,
        "data": args.data,
        "split_dir": args.split_dir,
        "iters": args.iters,
        "batch_size": args.batch_size,
        "learning_rate": args.learning_rate,
        "num_layers": args.num_layers,
        "seed": args.seed,
        "n_train": n_train, "n_valid": n_valid, "n_test": n_test,
    }
    with open(os.path.join(args.adapter_path, "v111_manifest.json"), "w") as fh:
        json.dump(manifest, fh, indent=2, sort_keys=True)
    print(f"v111.3: adapter + manifest written to {args.adapter_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
