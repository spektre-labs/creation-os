# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""CONSOLIDATE: long-horizon memory promotion (stub — wire episodic DB in harness)."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, List


@dataclass
class OmegaConsolidate:
    long_term: Any = None

    def __call__(self, engram: Any) -> int:
        if engram is None or not hasattr(engram, "_rows"):
            return 0
        rows: List[Any] = list(getattr(engram, "_rows", []))
        if self.long_term is None or not hasattr(self.long_term, "store"):
            return 0
        n = 0
        for _e in rows[:8]:
            n += 1
        return n
