# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""SAFETY: constitutional / Codex-style rule checks (lab stub)."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, List, Optional, Protocol, Tuple

from cos.sigma_gate_core import Verdict

from .state import OmegaContext


class Rule(Protocol):
    name: str

    def violated_by(self, text: str) -> bool: ...


@dataclass
class OmegaSafety:
    rules: Optional[List[Any]] = None

    def __post_init__(self) -> None:
        if self.rules is None:
            self.rules = []

    def __call__(self, ctx: OmegaContext, output: str) -> Tuple[Verdict, Optional[str]]:
        text = str(output or "")
        for rule in self.rules:
            if bool(rule.violated_by(text)):
                return Verdict.ABSTAIN, str(getattr(rule, "name", "rule"))
        return Verdict.ACCEPT, None
