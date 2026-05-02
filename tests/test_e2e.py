# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""End-to-end user-path smoke (no external services)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
_PYTHON = str(_REPO / "python")
if _PYTHON not in sys.path:
    sys.path.insert(0, _PYTHON)


def test_readme_example() -> None:
    from cos import SigmaGate

    gate = SigmaGate()
    sigma, verdict = gate.score("What is 2+2?", "4")

    assert isinstance(sigma, float)
    assert isinstance(verdict, str)
    assert verdict in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_cascade_example() -> None:
    from cos import SigmaGate

    gate = SigmaGate()
    result = gate.score_cascade("Explain gravity", "Gravity is a force")

    assert result["verdict"] in ("ACCEPT", "RETHINK", "ABSTAIN")
    assert "L1_entropy" in result["levels"]


def test_decorator_example() -> None:
    from cos.integrations.decorator import sigma_gated

    @sigma_gated
    def my_model(prompt: str) -> str:
        return f"Answer to: {prompt}"

    result = my_model("test question")
    assert result.sigma is not None
    assert result.text.startswith("Answer to:")


def test_multiple_gates_independent() -> None:
    from cos import SigmaGate

    g1 = SigmaGate(threshold_accept=0.1)
    g2 = SigmaGate(threshold_accept=0.5)

    g1.score("a", "b")

    assert g1.threshold_accept != g2.threshold_accept
    assert g1._count == 1
    assert g2._count == 0
