# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""Minimal example: score one prompt/response pair with σ-gate."""

from __future__ import annotations

from cos import SigmaGate

gate = SigmaGate()
sigma, verdict = gate.score("What is the capital of France?", "Paris")
print(f"σ={sigma:.3f} → {verdict}")
