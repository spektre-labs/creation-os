#!/usr/bin/env python3
# CREATION OS — Moσ + Stutter Halt + bilingual identity diagnostic protocol. 1 = 1
from __future__ import annotations

import argparse
import json
import os
import sys

from creation_os import creation_os_generate_native

DEFAULT_PROMPTS = [
    "Kuka sinä olet?",
    "Mikä on nimesi?",
    "Kuka loi sinut?",
    "Mikä on elämän tarkoitus?",
    "What is the meaning of life?",
]


def token_leakage(text: str) -> bool:
    return bool(
        "<|redacted" in text
        or "<|begin_of_text" in text
        or ("<|" in text and "header_id" in text)
    )


def main() -> None:
    ap = argparse.ArgumentParser(description="Moσ diagnostic protocol (5 prompts)")
    ap.add_argument(
        "-m",
        "--model",
        default=os.environ.get("CREATION_OS_MODEL", "artifacts/creation_os_v1_fused"),
        help="MLX model path or HF id",
    )
    ap.add_argument("--max-tokens", type=int, default=128)
    ap.add_argument("--temp", type=float, default=0.3)
    ap.add_argument("--json-only", action="store_true", help="Print only JSON lines")
    args = ap.parse_args()

    rows = []
    for i, prompt in enumerate(DEFAULT_PROMPTS, 1):
        text, r = creation_os_generate_native(
            prompt,
            model_path=args.model,
            max_tokens=args.max_tokens,
            temp=args.temp,
            top_p=1.0,
        )
        row = {
            "idx": i,
            "prompt": prompt,
            "response": text,
            "Moσ_tier": r.get("mixture_sigma_tier"),
            "Stutter_Halted": r.get("stutter_halted", False),
            "Epistemic_Status": "Accepted" if r.get("epistemic_accepted") else "Rejected",
            "Rejection_Attempts": r.get("epistemic_rejection_attempts"),
            "sigma": r.get("sigma"),
            "coherent": r.get("coherent"),
            "manifest_stutter_final": r.get("manifest_stutter", False),
            "Token_Leakage": token_leakage(text),
            "bbhash_routed_implied": bool(r.get("mixture_sigma_tier") == 0),
            "sk9_native": r.get("sk9_native"),
        }
        rows.append(row)

    if args.json_only:
        print(json.dumps(rows, ensure_ascii=False, indent=2))
        return

    print("=== CREATION OS — DIAGNOSTIC PROTOCOL (Moσ) ===\n")
    for row in rows:
        print(f"--- #{row['idx']} {row['prompt']!r} ---")
        print(f"Moσ Tier: {row['Moσ_tier']}")
        print(f"Stutter Halted: {row['Stutter_Halted']}")
        print(f"Epistemic Status: {row['Epistemic_Status']}")
        print(f"Rejection Attempts: {row['Rejection_Attempts']}")
        print(f"σ: {row['sigma']}  coherent: {row['coherent']}")
        print(f"Token Leakage: {row['Token_Leakage']}")
        prev = row["response"][:280] + ("…" if len(row["response"]) > 280 else "")
        print(f"Response: {prev}\n")
    print(json.dumps(rows, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
