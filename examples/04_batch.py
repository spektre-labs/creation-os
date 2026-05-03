# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""Batch scoring: several prompt/response pairs in one script."""

from __future__ import annotations

from cos import SigmaGate

gate = SigmaGate()
pairs = [
    ("2+2?", "4"),
    ("Capital of Japan?", "Tokyo"),
    ("Who painted the Mona Lisa?", "Picasso"),  # wrong answer → higher σ (lite entropy)
]
for prompt, response in pairs:
    sigma, verdict = gate.score(prompt, response)
    print(f"  {verdict:8s} σ={sigma:.3f}  {prompt} → {response}")
