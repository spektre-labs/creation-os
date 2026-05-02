# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""Local Qwen3.6 + σ-gate chat example.

Prerequisites:
  1. Run local OpenAI-compatible server (vLLM, SGLang, or e.g. llama.cpp
     ``llama-server``) exposing ``/v1/chat/completions``.
  2. ``pip install 'creation-os[openai]'``
  3. ``PYTHONPATH=python python examples/07_chat_local.py``

Adjust ``base_url`` and ``model`` to match your deployment.
"""
from __future__ import annotations

from openai import OpenAI

from cos import SigmaGate

client = OpenAI(base_url="http://localhost:8001/v1", api_key="local")
gate = SigmaGate()

messages: list[dict[str, str]] = []
print("Creation OS + Qwen3.6 (type 'quit' to exit)")
while True:
    user = input("\n> ")
    if user.strip().lower() in ("quit", "exit"):
        break
    messages.append({"role": "user", "content": user})
    resp = client.chat.completions.create(
        model="Qwen3.6-35B-A3B",
        messages=messages,
        extra_body={"chat_template_kwargs": {"preserve_thinking": True}},
    )
    text = resp.choices[0].message.content or ""
    sigma, verdict = gate.score(user, text)
    messages.append({"role": "assistant", "content": text})
    print(f"\n[σ={sigma:.3f} {verdict}]")
    print(text)
