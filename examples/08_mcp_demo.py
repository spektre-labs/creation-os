# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""MCP server demo — test σ-gate tools locally.

Start MCP (stdio)::

    pip install 'creation-os[mcp]'
    cos mcp

Or run this script to simulate the same scoring paths without spawning MCP::

    python examples/08_mcp_demo.py
"""
from __future__ import annotations

from cos.sigma_gate import SigmaGate

gate = SigmaGate()

pairs = [
    ("What is 2+2?", "4"),
    ("Capital of France?", "Paris"),
    ("Who painted Mona Lisa?", "Picasso"),
]

print("σ-gate MCP tool simulation:\n")
for prompt, response in pairs:
    sigma, verdict = gate.score(prompt, response)
    print(f"  {verdict:8s} σ={sigma:.3f}  {prompt} → {response}")
