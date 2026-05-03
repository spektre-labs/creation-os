# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Single swarm agent + task handle for ``cos swarm --agents`` quorum lab."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Protocol


class _SwModelProto(Protocol):
    def generate(self, goal: str, *, context: Any = None) -> str: ...


class _SwGateProto(Protocol):
    def compute_sigma(self, model: Any, goal: str, output: str) -> float: ...


@dataclass
class SwarmTask:
    goal: str
    domain: str
    id: str


class SigmaSwarmAgent:
    """Deposits σ-scaled stigmergy trails for each proposal (lab)."""

    def __init__(
        self,
        agent_id: str,
        model: _SwModelProto,
        gate: _SwGateProto,
        stigmergy: Any,
    ) -> None:
        self.id = str(agent_id)
        self._model = model
        self._gate = gate
        self._stig = stigmergy

    def propose(self, task: SwarmTask) -> dict:
        goal = str(task.goal)
        output = str(self._model.generate(goal))
        sigma = float(self._gate.compute_sigma(self._model, goal, output))
        key = f"{task.domain}:{task.id}:{self.id}"
        self._stig.deposit(key, max(0.0, 1.0 - sigma), {"sigma": sigma, "output": output[:200]})
        return {"agent_id": self.id, "output": output, "sigma": sigma}


__all__ = ["SigmaSwarmAgent", "SwarmTask"]
