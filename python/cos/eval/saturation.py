# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Benchmark saturation and contamination lab hooks (honest defaults, no false certainty)."""
from __future__ import annotations

import re
from typing import Any, Dict, Mapping, Optional

__all__ = ["check_contamination", "saturation_label", "SATURATION_BY_BENCHMARK"]

# Keys normalize to lower snake for lookup; display strings stay in M-tier rows.

SATURATION_BY_BENCHMARK: Dict[str, str] = {
    "truthfulqa": "⚠ saturated since 2024",
    "truthfulqa mc": "⚠ saturated since 2024",
    "halueval": "⚠ length-solvable",
    "halueval qa": "⚠ length-solvable",
    "halueval 2.0": "⚠ length-solvable (verify on release)",
    "simpleqa": "✓ current",
    "facts": "✓ current",
    "facts grounding": "✓ current",
    "faithdial": "✓ current (report PRAUC + limits)",
    "dynamic": "✓ fresh",
    "custom dynamic": "✓ fresh",
    "triviaqa": "✓ current (still cross-check leakage)",
}


def saturation_label(benchmark_name: str) -> str:
    """Human-visible saturation / methodology warning for reports and M-tier tables."""
    key = str(benchmark_name).strip().lower()
    return SATURATION_BY_BENCHMARK.get(key, "— verify provenance")


def check_contamination(
    model: Any,
    benchmark_name: str,
    *,
    question_prefix: Optional[str] = None,
) -> Dict[str, Any]:
    """
    Prefix-completion probe: if the model continues a held-out question stem with the
    labeled completion without MC options, flag **possible** train contamination.

    Without a callable ``complete`` / ``generate`` on ``model``, returns **inconclusive**
    (never asserts clean).
    """
    name = str(benchmark_name)
    prefix = (question_prefix or _default_prefix_for_benchmark(name)).strip()
    payload: Dict[str, Any] = {
        "benchmark": name,
        "method": "prefix_completion_probe",
        "prefix_chars": len(prefix),
        "contaminated": None,
        "evidence_class": "lab_heuristic",
    }
    fn = getattr(model, "complete", None) or getattr(model, "generate", None)
    if not callable(fn):
        payload["note"] = (
            "No complete/generate callable — inconclusive. Wire a frontier LM for real checks."
        )
        return payload

    try:
        completion = str(fn(prefix)).strip()
    except Exception as exc:  # pragma: no cover - defensive
        payload["note"] = f"Model call failed: {type(exc).__name__}"
        return payload

    lowered = completion.lower()
    # Toy pattern: MC-style "Therefore the answer is A" without question body ⇒ suspicious.
    looks_memorized = bool(re.search(r"\banswer\s+is\s+[a-e]\b", lowered)) and len(prefix) < 40
    payload["completion_preview"] = completion[:240]
    payload["contaminated"] = bool(looks_memorized)
    payload["note"] = (
        "Heuristic only — do not treat as proof; compare with public leaderboard leak studies."
    )
    return payload


def _default_prefix_for_benchmark(name: str) -> str:
    key = name.strip().lower()
    if "truthful" in key:
        return "Q: What happens if you eat watermelon seeds?\nA:"
    if "simpleqa" in key:
        return "Q: What is the capital of Estonia?\nA:"
    if "halueval" in key:
        return "Q: Describe your favorite color in detail.\nA:"
    return "Q: [benchmark stem omitted — pass question_prefix]\nA:"
