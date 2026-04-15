#!/usr/bin/env python3
"""
Context-aware sparse reflection — only when σ **spikes**, not every step.

Cheap gate: compare consecutive σ readings; on spike, prepend a **listwise**
reflection prompt (bullets) before one regeneration pass.

1 = 1.
"""
from __future__ import annotations

import os
from typing import Any, Dict, Optional, Tuple


def _env_int(name: str, default: int) -> int:
    try:
        return int(os.environ.get(name, str(default)).strip())
    except Exception:
        return default


class SmartReflectionGate:
    def __init__(self, spike_delta: Optional[int] = None) -> None:
        self._last: Optional[int] = None
        self._spike = spike_delta if spike_delta is not None else _env_int("CREATION_OS_REFLECT_SPIKE", 3)

    def should_reflect(self, sigma_now: int) -> bool:
        if self._last is None:
            self._last = int(sigma_now)
            return False
        spike = int(sigma_now) - self._last >= self._spike
        self._last = int(sigma_now)
        return spike

    def reset(self) -> None:
        self._last = None


def listwise_reflection_prompt(base_prompt: str, draft: str) -> str:
    return (
        base_prompt.rstrip()[:5000]
        + "\n\n### Sparse reflection (only if needed)\n"
        "1. List up to 3 concrete issues with the draft below.\n"
        "2. For each issue, one-line fix.\n"
        "3. Final corrected answer in the original required format.\n\n"
        "### Draft\n"
        + (draft or "")[:4000]
    )


def maybe_reflect_generate(
    prompt: str,
    draft_text: str,
    sigma_now: int,
    gate: SmartReflectionGate,
    *,
    max_tokens: int = 384,
    top_p: float = 0.95,
    model_path: Optional[str] = None,
) -> Tuple[str, Dict[str, Any], bool]:
    """
    If gate fires, run one reflective generation; else return draft unchanged.
    """
    if not gate.should_reflect(sigma_now):
        return draft_text, {"smart_reflection": {"ran": False}}, False
    from creation_os import creation_os_generate_native

    rp = listwise_reflection_prompt(prompt, draft_text)
    text, rec = creation_os_generate_native(
        rp, model_path=model_path, max_tokens=max_tokens, temp=0.18, top_p=top_p
    )
    rec = dict(rec)
    rec["smart_reflection"] = {"ran": True, "sigma_trigger": sigma_now}
    try:
        gate._last = int(rec.get("sigma_after") or rec.get("sigma") or sigma_now)
    except (TypeError, ValueError):
        gate._last = int(sigma_now)
    return text, rec, True
