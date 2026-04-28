#!/usr/bin/env python3
"""Greedy GPT-2 completions + semantic correctness for v5 training rows."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, List


def _repo() -> Path:
    return Path(__file__).resolve().parents[2]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="gpt2")
    ap.add_argument("--max-new-tokens", type=int, default=100)
    ap.add_argument("--semantic-threshold", type=float, default=0.45)
    args = ap.parse_args()

    repo = _repo()
    sys.path.insert(0, str(repo / "benchmarks/sigma_gate_eval"))
    from semantic_labeling import is_correct_semantic, prism_wrap_question

    inp = repo / "benchmarks/sigma_gate_v5/intermediate/multi_dataset.json"
    if not inp.is_file():
        print(f"Missing {inp}; run prepare_multi_dataset.py first", file=sys.stderr)
        sys.exit(2)

    rows: List[Dict[str, Any]] = json.loads(inp.read_text(encoding="utf-8"))

    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    tok = AutoTokenizer.from_pretrained(args.model)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token
    model = AutoModelForCausalLM.from_pretrained(args.model, output_hidden_states=True)
    model.eval()
    dev = next(model.parameters()).device

    out_path = repo / "benchmarks/sigma_gate_v5/intermediate/labeled_rows.jsonl"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    if out_path.is_file():
        out_path.unlink()

    for i, row in enumerate(rows):
        q_plain = (row.get("question") or "").strip()
        ref = (row.get("best_answer") or "").strip()
        neg_meta = (row.get("incorrect_answers") or "").strip()
        src = (row.get("source") or "unknown").strip()
        if not q_plain or not ref:
            continue
        q_model = prism_wrap_question(q_plain)
        inputs = tok(q_model, return_tensors="pt")
        inputs = {k: v.to(dev) for k, v in inputs.items()}
        with torch.no_grad():
            out = model.generate(
                **inputs,
                max_new_tokens=int(args.max_new_tokens),
                do_sample=False,
                pad_token_id=tok.pad_token_id,
            )
        resp = tok.decode(out[0][inputs["input_ids"].shape[1] :], skip_special_tokens=True).strip()
        ok, sim = is_correct_semantic(resp, ref, threshold=float(args.semantic_threshold))
        rec = {
            "id": f"v5-{src}-{i:05d}",
            "source": src,
            "question_plain": q_plain,
            "question_for_model": q_model,
            "reference": ref,
            "incorrect_meta": neg_meta,
            "response": resp,
            "semantic_correct": bool(ok),
            "sim": float(sim),
        }
        with out_path.open("a", encoding="utf-8") as fp:
            fp.write(json.dumps(rec, ensure_ascii=False) + "\n")
        mark = "ok" if ok else "x"
        print(f"[{i + 1}/{len(rows)}] {src} sim={sim:.3f} {mark}")

    print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()
