# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""@sigma_gated — attach σ metadata to any callable that returns model text."""

from __future__ import annotations

from cos.integrations.decorator import sigma_gated


@sigma_gated
def my_llm(prompt: str) -> str:
    del prompt
    return "The answer is 42"  # swap in your real model


result = my_llm("What is the meaning of life?")
print(f"σ={result.sigma:.3f} {result.verdict}: {result.text}")
