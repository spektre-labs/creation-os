#!/usr/bin/env python3
"""
Guided-by-gut — internal signals as search prior (token σ trace + diversity).

Without a second LLM judge, use **per-token σ steps** and **token-id diversity**
as cheap proxies for confidence / novelty. Feeds orchestrators (temperature bump,
Wait injection, branch expand).

1 = 1.
"""
from __future__ import annotations

from typing import Any, Dict, List


def gut_metrics(receipt: Dict[str, Any]) -> Dict[str, float]:
    trace: List[Dict[str, Any]] = list(receipt.get("sigma_per_token") or [])
    if not trace:
        return {
            "confidence_proxy": 0.45,
            "novelty_proxy": 0.0,
            "sigma_mean": 0.0,
            "sigma_max": 0.0,
            "steps": 0,
        }
    sigs: List[int] = []
    tids: List[int] = []
    for step in trace:
        sa = step.get("sigma_after")
        if sa is not None:
            try:
                sigs.append(int(sa))
            except (TypeError, ValueError):
                pass
        tid = step.get("token_id")
        if tid is not None:
            try:
                tids.append(int(tid))
            except (TypeError, ValueError):
                pass
    if not sigs:
        sigs = [0]
    smax = max(sigs)
    smin = min(sigs)
    smean = sum(sigs) / len(sigs)
    stability = 1.0 - (smax - smin) / max(1, smax + 1)
    confidence_proxy = max(0.0, min(1.0, 1.0 - smean / max(8.0, smax + 1.0) + 0.25 * stability))
    novelty_proxy = (len(set(tids)) / max(1, len(tids))) if tids else 0.0
    return {
        "confidence_proxy": round(confidence_proxy, 4),
        "novelty_proxy": round(novelty_proxy, 4),
        "sigma_mean": round(smean, 4),
        "sigma_max": float(smax),
        "steps": float(len(trace)),
    }


def suggest_temperature_adjustment(base_temp: float, metrics: Dict[str, float]) -> float:
    """Lower confidence / novelty → slightly higher temp for next sample."""
    t = float(base_temp)
    if metrics["confidence_proxy"] < 0.35:
        t += 0.08
    if metrics["novelty_proxy"] < 0.12:
        t += 0.05
    return max(0.05, min(1.35, t))


def receipt_with_gut(receipt: Dict[str, Any]) -> Dict[str, Any]:
    out = dict(receipt)
    out["guided_by_gut"] = gut_metrics(receipt)
    return out
