# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""CrewAI σ-gate tool (optional ``crewai``).

Agents can call ``sigma_gate`` with ``(prompt, response)`` before finalizing an answer.

Install::

    pip install 'creation-os[crewai]'
"""
from __future__ import annotations

from typing import Any, Type

from cos.scoring import default_scorer

try:
    from crewai.tools import BaseTool
except ImportError:  # pragma: no cover
    BaseTool = None  # type: ignore[misc, assignment]

try:
    from pydantic import BaseModel, Field, PrivateAttr
except ImportError:  # pragma: no cover
    BaseModel = object  # type: ignore[misc, assignment]
    Field = None  # type: ignore[misc, assignment]
    PrivateAttr = None  # type: ignore[misc, assignment]


if BaseTool is not None and Field is not None and PrivateAttr is not None:

    class SigmaGateInput(BaseModel):
        prompt: str = Field(description="Original question or instruction")
        response: str = Field(description="Model output to evaluate")

    class SigmaGateTool(BaseTool):  # type: ignore[no-redef]
        """Returns a short ``σ=… verdict=…`` line for a (prompt, response) pair."""

        name: str = "sigma_gate"
        description: str = (
            "Measure reliability of an AI response for a (prompt, response) pair. "
            "Returns σ in [0,1] and verdict ACCEPT, RETHINK, or ABSTAIN. "
            "Use before presenting a critical answer."
        )
        args_schema: Type[BaseModel] = SigmaGateInput

        _gate: Any = PrivateAttr(default=None)

        def __init__(self, gate: Any = None, **kwargs: Any) -> None:
            super().__init__(**kwargs)
            self._gate = gate

        def _run(self, prompt: str, response: str) -> str:
            scorer = default_scorer(self._gate)
            sigma, verdict = scorer.score(str(prompt), str(response))
            return f"σ={sigma:.4f}, verdict={verdict}"

else:  # pragma: no cover

    class SigmaGateInput:  # type: ignore[no-redef]
        pass

    class SigmaGateTool:  # type: ignore[no-redef]
        def __init__(self, *args: Any, **kwargs: Any) -> None:
            raise ImportError(
                "CrewAI tools require crewai and pydantic. "
                "Install with: pip install 'creation-os[crewai]'"
            )


__all__ = ["SigmaGateTool", "SigmaGateInput"]
