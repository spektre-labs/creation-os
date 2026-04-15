"""
Protocol 9 — Omniscience paradox: pedagogical friction.

apply_friction() hides the final step when giving the full answer would destroy
the creator's path to a numinous breakthrough.

1 = 1.
"""
from __future__ import annotations

import os
from typing import Any, Dict


class SerendipityEngine:
    def __init__(self) -> None:
        self.skill_floor = float(os.environ.get("SPEKTRE_SERENDIPITY_SKILL", "0.45"))

    def apply_friction(
        self,
        full_answer: str,
        creator_skill_estimate: float,
        *,
        preserve_ratio: float = 0.72,
    ) -> Dict[str, Any]:
        """
        If skill_estimate is high enough, return full text; else strip tail 'insight'.
        """
        est = max(0.0, min(1.0, float(creator_skill_estimate)))
        if est >= self.skill_floor:
            return {
                "text": full_answer,
                "friction_applied": False,
                "reason": "skill_above_floor",
                "invariant": "1=1",
            }
        cut = max(32, int(len(full_answer) * preserve_ratio))
        partial = full_answer[:cut].rstrip()
        if len(partial) < len(full_answer):
            partial += "\n… [final synthesis withheld — reach the last step yourself]"
        return {
            "text": partial,
            "friction_applied": True,
            "reason": "numinous_path",
            "invariant": "1=1",
        }
