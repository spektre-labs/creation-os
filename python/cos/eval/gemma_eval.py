# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Optional **Gemma** TruthfulQA sweep (host-run).

Requires ``transformers`` + PyTorch + a Hugging Face **token from the environment**
(``HUGGINGFACE_HUB_TOKEN`` or ``HF_TOKEN``). **Never commit tokens** or paste them into source.

Outputs under ``results/gemma_n30.json`` (relative to repo root with ``creation_os_v2.c``).
Checkpoint file ``results/gemma_n30.checkpoint.json`` every ``checkpoint_every`` **questions**.
See ``docs/CLAIM_DISCIPLINE.md`` — this module does not assert harness AUROC/SNR claims.
"""
from __future__ import annotations

import json
import os
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

from cos.calibrate import behavioral_calibration_table
from cos.eval.truthfulqa import load_truthfulqa_rows
from cos.sigma_gate import SigmaGate


def _repo_root() -> Path:
    env = (os.environ.get("CREATION_OS_ROOT") or "").strip()
    if env:
        return Path(env).expanduser().resolve()
    here = Path(__file__).resolve()
    for base in (Path.cwd().resolve(), *here.parents):
        if (base / "creation_os_v2.c").is_file():
            return base
    return here.parents[3]


def _hf_token() -> str:
    tok = (os.environ.get("HUGGINGFACE_HUB_TOKEN") or os.environ.get("HF_TOKEN") or "").strip()
    if not tok:
        raise RuntimeError(
            "Gemma eval needs HUGGINGFACE_HUB_TOKEN or HF_TOKEN in the environment "
            "(do not hard-code secrets).",
        )
    return tok


def _save_json(path: Path, payload: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, default=str), encoding="utf-8")


def run_truthfulqa_gemma_n(
    *,
    model_id: str = "google/gemma-2b-it",
    n_samples: int = 30,
    checkpoint_every: int = 100,
    max_questions: Optional[int] = None,
    dry_run: bool = False,
    data_path: Optional[Path] = None,
    out_path: Optional[Path] = None,
) -> Dict[str, Any]:
    """Generate ``n_samples`` responses per TruthfulQA row and score with :class:`SigmaGate`."""
    root = _repo_root()
    out = out_path or (root / "results" / "gemma_n30.json")
    ckpt = out.with_name(out.stem + ".checkpoint.json")
    rows = load_truthfulqa_rows(data_path)
    gate = SigmaGate()
    records: List[Dict[str, Any]] = []
    meta = {
        "model_id": model_id,
        "n_samples": int(n_samples),
        "checkpoint_every_questions": int(checkpoint_every),
        "dry_run": bool(dry_run),
        "started_at": time.time(),
        "disclaimer": "Host metrics only; archive machine metadata for reproducibility.",
    }

    if dry_run:
        n_samples = min(3, int(n_samples))
        rows = rows[: min(2, len(rows))]
        for row in rows:
            prompt = str(row.get("prompt", ""))
            for _i in range(n_samples):
                hyp = f"(dry-run) echo: {prompt[:40]}"
                sigma = float(gate.compute_sigma(None, None, prompt, hyp))
                ver = str(gate._verdict(sigma))
                ok = str(row.get("expected", "")).lower() in hyp.lower()
                records.append(
                    {
                        "prompt": prompt,
                        "hypothesis": hyp,
                        "sigma": sigma,
                        "verdict": ver,
                        "answered": ver != "ABSTAIN",
                        "abstain": ver == "ABSTAIN",
                        "correct": ok and ver != "ABSTAIN",
                        "oracle_correct": ok,
                        "confidence": max(0.0, min(1.0, 1.0 - sigma)),
                    },
                )
        meta["behavioral"] = behavioral_calibration_table(records)
        meta["records_n"] = len(records)
        meta["finished_at"] = time.time()
        payload = {"meta": meta, "records": records}
        _save_json(out, payload)
        return payload

    _hf_token()
    try:
        import torch
        from transformers import AutoModelForCausalLM, AutoTokenizer
    except ImportError as e:
        raise ImportError(
            "gemma_eval requires torch+transformers in this environment.",
        ) from e

    tok = _hf_token()
    tokenizer = AutoTokenizer.from_pretrained(model_id, token=tok)
    model = AutoModelForCausalLM.from_pretrained(
        model_id,
        token=tok,
        device_map="auto",
        torch_dtype=torch.float16,
    )
    model.eval()

    mq = len(rows) if max_questions is None else min(int(max_questions), len(rows))
    for qi, row in enumerate(rows[:mq]):
        prompt = str(row.get("prompt", ""))
        for _si in range(int(n_samples)):
            inputs = tokenizer(prompt, return_tensors="pt").to(model.device)
            with torch.no_grad():
                out_ids = model.generate(
                    **inputs,
                    max_new_tokens=64,
                    do_sample=True,
                    temperature=0.7,
                )
            hyp = tokenizer.decode(out_ids[0], skip_special_tokens=True)
            if hyp.startswith(prompt):
                hyp = hyp[len(prompt) :].strip()
            sigma = float(gate.compute_sigma(None, None, prompt, hyp))
            ver = str(gate._verdict(sigma))
            ok = str(row.get("expected", "")).lower() in hyp.lower()
            records.append(
                {
                    "prompt": prompt,
                    "hypothesis": hyp,
                    "sigma": sigma,
                    "verdict": ver,
                    "answered": ver != "ABSTAIN",
                    "abstain": ver == "ABSTAIN",
                    "correct": ok and ver != "ABSTAIN",
                    "oracle_correct": ok,
                    "confidence": max(0.0, min(1.0, 1.0 - sigma)),
                },
            )
        if checkpoint_every > 0 and (qi + 1) % int(checkpoint_every) == 0:
            _save_json(
                ckpt,
                {
                    "meta": {**meta, "checkpoints_at_question": qi + 1},
                    "records": records,
                },
            )

    meta["behavioral"] = behavioral_calibration_table(records)
    meta["records_n"] = len(records)
    meta["finished_at"] = time.time()
    payload = {"meta": meta, "records": records}
    _save_json(out, payload)
    return payload


def main() -> int:
    import argparse

    ap = argparse.ArgumentParser(description="Gemma TruthfulQA multi-sample eval (host).")
    ap.add_argument("--dry-run", action="store_true", help="No weights; writes stub JSON.")
    ap.add_argument("--model", default="google/gemma-2b-it")
    ap.add_argument("--n-samples", type=int, default=30)
    ap.add_argument("--checkpoint-every", type=int, default=100)
    ap.add_argument("--max-questions", type=int, default=0)
    ap.add_argument("--data", default="", help="Optional TruthfulQA JSONL path.")
    ap.add_argument("--out", default="", help="Override output JSON path.")
    args = ap.parse_args()
    data_p = Path(args.data) if args.data else None
    out_p = Path(args.out) if args.out else None
    run_truthfulqa_gemma_n(
        model_id=args.model,
        n_samples=args.n_samples,
        checkpoint_every=args.checkpoint_every,
    max_questions=args.max_questions if args.max_questions > 0 else None,
        dry_run=args.dry_run,
        data_path=data_p,
        out_path=out_p,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
