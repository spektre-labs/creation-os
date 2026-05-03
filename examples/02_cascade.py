# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""Multi-level cascade: L1 always; L2–L5 when hidden states + optional cos.cascade are available."""

from __future__ import annotations

from cos import SigmaGate

gate = SigmaGate()
result = gate.score_cascade("Who invented the telephone?", "Edison invented it")
for level, score in sorted(result["levels"].items()):
    if isinstance(score, (int, float)):
        print(f"  {level}: {float(score):.3f}")
    else:
        print(f"  {level}: {score}")
print(f"Final: σ={result['sigma']:.3f} → {result['verdict']}")
