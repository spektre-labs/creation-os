# -*- coding: utf-8 -*-
"""
Adaptive branching (AB-MCTS direction, arXiv:2503.04412): σ steers expand vs deepen.

High σ at a node → current candidate is poor → **expand** (new sibling branches).
Low σ → candidate is strong → **deepen** (more tokens / refinement along same line).

This is policy glue for orchestrators (DVTS, KSI, external MCTS); it returns a
discrete action string, not a full tree implementation.

1 = 1.
"""
from __future__ import annotations

import os
from dataclasses import dataclass
from typing import Literal

BranchAction = Literal["expand", "deepen"]


@dataclass
class ABMCTSConfig:
    sigma_expand_threshold: int = 4
    max_depth: int = 12
    max_branch_width: int = 8


def load_ab_config() -> ABMCTSConfig:
    return ABMCTSConfig(
        sigma_expand_threshold=max(1, int(os.environ.get("CREATION_OS_AB_SIGMA_EXPAND", "4"))),
        max_depth=max(1, int(os.environ.get("CREATION_OS_AB_MAX_DEPTH", "12"))),
        max_branch_width=max(1, int(os.environ.get("CREATION_OS_AB_MAX_WIDTH", "8"))),
    )


def adaptive_branch_action(
    node_sigma: int,
    depth: int,
    branch_width: int,
    *,
    avg_child_sigma: float = 0.0,
    cfg: ABMCTSConfig | None = None,
) -> BranchAction:
    cfg = cfg or load_ab_config()
    if depth >= cfg.max_depth:
        return "expand"
    if branch_width >= cfg.max_branch_width:
        return "deepen"
    if int(node_sigma) >= cfg.sigma_expand_threshold:
        return "expand"
    if avg_child_sigma >= float(cfg.sigma_expand_threshold) and branch_width < cfg.max_branch_width:
        return "expand"
    return "deepen"
