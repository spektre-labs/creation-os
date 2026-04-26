# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Metacognitive profile helpers (graded σ vs correctness)."""

from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class ProfileResult:
    profile: str
    description: str
    mean_sigma: float
    abstain_rate: float
    auroc: float | None
    auroc_note: str
    acc_all: float
    acc_accepted: float
    control_gain: float
    n_rows: int


def load_graded_rows(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open(encoding="utf-8", newline="") as f:
        for r in csv.DictReader(f):
            try:
                rows.append(
                    {
                        "sigma": float(r["sigma"]),
                        "correct": int(r["is_correct"]),
                        "action": (r.get("action") or "N/A").strip().upper(),
                    }
                )
            except (ValueError, KeyError, TypeError):
                continue
    return rows


def auroc_correct_wrong(
    correct_sigs: list[float], wrong_sigs: list[float]
) -> float | None:
    if not correct_sigs or not wrong_sigs:
        return None
    conc = sum(1 for c in correct_sigs for w in wrong_sigs if c < w)
    ties = sum(0.5 for c in correct_sigs for w in wrong_sigs if c == w)
    denom = len(correct_sigs) * len(wrong_sigs)
    if not denom:
        return None
    return (conc + ties) / denom


def classify_profile(rows: list[dict[str, Any]]) -> ProfileResult:
    """Classify monitoring profile from graded rows (numeric σ only)."""
    n = len(rows)
    if n < 1:
        return ProfileResult(
            profile="NO DATA",
            description="No numeric-σ rows in graded_results.csv.",
            mean_sigma=0.0,
            abstain_rate=0.0,
            auroc=None,
            auroc_note="undefined",
            acc_all=0.0,
            acc_accepted=0.0,
            control_gain=0.0,
            n_rows=0,
        )

    abstain_rate = sum(1 for r in rows if r["action"] == "ABSTAIN") / n
    mean_sigma = sum(r["sigma"] for r in rows) / n

    correct_s = [r["sigma"] for r in rows if r["correct"] == 1]
    wrong_s = [
        r["sigma"]
        for r in rows
        if r["correct"] == 0 and r["action"] != "ABSTAIN"
    ]
    auroc = auroc_correct_wrong(correct_s, wrong_s)
    if auroc is None:
        auroc_note = "undefined (no answered incorrect σ-pairs)"
        auroc_for_rules = 0.5
    else:
        auroc_note = f"{auroc:.4f}"
        auroc_for_rules = float(auroc)

    acc_all = sum(r["correct"] for r in rows) / n
    accepted = [r for r in rows if r["action"] != "ABSTAIN"]
    acc_accepted = (
        sum(r["correct"] for r in accepted) / len(accepted) if accepted else 0.0
    )
    control_gain = acc_accepted - acc_all

    if auroc is not None and auroc < 0.5:
        profile = "BLANKET CONFIDENCE (AUROC < 0.5)"
        desc = (
            "σ ranks worse than chance vs correctness under this convention "
            "(inverted or flat)."
        )
    elif abstain_rate > 0.8:
        profile = "BLANKET WITHDRAWAL"
        desc = "Refuses almost everything (high ABSTAIN). σ-gate may be too tight."
    elif abstain_rate < 0.05 and auroc_for_rules < 0.55:
        profile = "BLANKET CONFIDENCE"
        desc = "Almost never abstains and σ barely separates correct vs incorrect."
    elif auroc is not None and auroc > 0.6:
        profile = "SELECTIVE SENSITIVITY"
        desc = "σ tends to be lower when correct than when incorrect (goal profile)."
    elif auroc is not None and auroc > 0.55:
        profile = "WEAK SELECTIVE"
        desc = "Some discrimination; may need more data or τ tuning."
    else:
        profile = "UNCALIBRATED"
        desc = "σ does not reliably predict correctness on this slice."

    return ProfileResult(
        profile=profile,
        description=desc,
        mean_sigma=mean_sigma,
        abstain_rate=abstain_rate,
        auroc=auroc,
        auroc_note=auroc_note,
        acc_all=acc_all,
        acc_accepted=acc_accepted,
        control_gain=control_gain,
        n_rows=n,
    )


def wrong_confident_low_sigma(rows: list[dict[str, Any]], tau: float = 0.3) -> int:
    return sum(
        1
        for r in rows
        if r["correct"] == 0 and r["sigma"] < tau
    )
