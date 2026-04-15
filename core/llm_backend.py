#!/usr/bin/env python3
"""
ALM Tier-2 backend: MLX Llama (optional). Kutsu: python3 llm_backend.py /path/to/question.txt
Tuloste stdoutiin (yksi vastaus, ilman ylimääräistä JSONia).
"""
from __future__ import annotations

import sys
from pathlib import Path


def main() -> None:
    if len(sys.argv) < 2:
        sys.stderr.write("usage: llm_backend.py <question_file>\n")
        sys.exit(2)
    qpath = Path(sys.argv[1])
    question = qpath.read_text(encoding="utf-8", errors="replace").strip()

    model_id = (
        sys.environ.get("ALM_MLX_MODEL")
        or "mlx-community/Llama-3.2-3B-Instruct-4bit"
    )

    try:
        from mlx_lm import generate, load
    except ImportError as e:
        print(
            f"(stub) MLX ei käytössä ({e}). Kysymys ({len(question)} merkkiä) vastaanotettu.",
            end="",
        )
        return

    try:
        model, tokenizer = load(model_id)
    except Exception as e:
        print(f"(stub) Mallin lataus epäonnistui ({e}).", end="")
        return

    prompt = (
        "You are a precise factual assistant.\n"
        "If you are not certain, say you do not know.\n"
        "Never guess or fabricate.\n\n"
        f"Question: {question}\nAnswer:"
    )
    try:
        out = generate(
            model,
            tokenizer,
            prompt=prompt,
            max_tokens=min(200, int(sys.environ.get("ALM_MAX_TOKENS", "200"))),
            temp=float(sys.environ.get("ALM_TEMP", "0.1")),
        )
        print(out.strip(), end="")
    except Exception as e:
        print(f"(stub) generate epäonnistui: {e}", end="")


if __name__ == "__main__":
    main()
