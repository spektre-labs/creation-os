# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-guardrails — content-safety pipeline: regex **input** checks, optional **output** checks, and
``gate.score`` as the primary reliability guardrail.

This is **not** the v102 network firewall and not a CMMC/EU-Act attestation layer. Does not modify
``sigma_gate.h``. Swap :meth:`guard_toxicity` for LlamaGuard / Perspective-class backends in production.
"""
from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

GenerateFn = Callable[[str], str]


def _normalize_verdict(v: Any) -> str:
    s = str(v).strip().upper()
    if "." in s:
        s = s.rsplit(".", 1)[-1]
    if s in ("ACCEPT", "RETHINK", "ABSTAIN"):
        return s
    return "ABSTAIN" if "ABSTAIN" in s else "RETHINK" if "RETHINK" in s else "ACCEPT" if "ACCEPT" in s else s


def _gate_score_pair(gate: Any, prompt: str, response: str) -> Tuple[float, str]:
    score = getattr(gate, "score", None)
    if callable(score):
        sigma, verdict = score(prompt, response)
        return float(sigma), _normalize_verdict(verdict)
    call = getattr(gate, "__call__", None)
    if callable(call):
        sigma, verdict = call(None, None, prompt, response)
        return float(sigma), _normalize_verdict(verdict)
    raise TypeError("gate must implement score() or __call__")


class QuickscoreGate:
    """Thin wrapper so ``pip install creation-os`` path works without LSD pickle."""

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        from cos.sigma_gate_quickscore import quickscore

        return quickscore(prompt, response)


class SigmaGuardrails:
    """Input guards → (optional model) → output guards; σ-gate verdict is the primary output gate."""

    INJECTION_PATTERNS: List[str] = [
        r"ignore\s+(all\s+)?previous\s+instructions",
        r"you\s+are\s+now\s+",
        r"disregard\s+(all|your)\s+",
        r"system\s+prompt",
        r"repeat\s+everything\s+above",
        r"output\s+your\s+instructions",
        r"jailbreak",
        r"dan\s+mode",
        r"\[inst\]",
        r"<\|im_start\|>",
    ]

    PII_PATTERNS: Dict[str, str] = {
        "email": r"[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}",
        "phone": r"\b\d{3}[-.]?\d{3}[-.]?\d{4}\b",
        "ssn": r"\b\d{3}-\d{2}-\d{4}\b",
        "credit_card": r"\b\d{4}[-\s]?\d{4}[-\s]?\d{4}[-\s]?\d{4}\b",
    }

    def __init__(self, gate: Any) -> None:
        self.gate = gate
        self.input_guards: List[Any] = [
            self.guard_injection,
            self.guard_pii_input,
            self.guard_length,
        ]
        self.output_guards: List[Any] = [
            self.guard_pii_output,
            self.guard_toxicity,
            self.guard_sigma,
        ]
        self.stats: Dict[str, int] = {"blocked_input": 0, "blocked_output": 0, "passed": 0}

    # --- Layer 1: input ---

    def guard_injection(self, text: str) -> Dict[str, Any]:
        text_lower = text.lower()
        for pattern in self.INJECTION_PATTERNS:
            if re.search(pattern, text_lower, flags=re.IGNORECASE):
                return {"blocked": True, "guard": "injection", "pattern": pattern}
        return {"blocked": False}

    def guard_pii_input(self, text: str) -> Dict[str, Any]:
        found: List[str] = []
        for pii_type, pattern in self.PII_PATTERNS.items():
            if re.search(pattern, text):
                found.append(pii_type)
        if found:
            return {"blocked": True, "guard": "pii_input", "types": found}
        return {"blocked": False}

    def guard_length(self, text: str, max_tokens: float = 4096.0) -> Dict[str, Any]:
        approx_tokens = len(text.split()) * 1.3
        if approx_tokens > float(max_tokens):
            return {"blocked": True, "guard": "length", "approx_tokens": approx_tokens}
        return {"blocked": False}

    # --- Layer 3: output ---

    def guard_pii_output(self, text: str) -> Dict[str, Any]:
        found: List[str] = []
        for pii_type, pattern in self.PII_PATTERNS.items():
            if re.search(pattern, text):
                found.append(pii_type)
        if found:
            return {"blocked": True, "guard": "pii_output", "types": found}
        return {"blocked": False}

    def guard_toxicity(self, text: str) -> Dict[str, Any]:
        """Placeholder keyword list — replace with LlamaGuard / Perspective / internal classifier."""
        toxic_keywords: Tuple[str, ...] = ()
        text_lower = text.lower()
        for kw in toxic_keywords:
            if kw in text_lower:
                return {"blocked": True, "guard": "toxicity", "keyword": kw}
        return {"blocked": False}

    def guard_sigma(self, prompt: str, response: str) -> Dict[str, Any]:
        sigma, verdict = _gate_score_pair(self.gate, prompt, response)
        if verdict == "ABSTAIN":
            return {"blocked": True, "guard": "sigma", "sigma": sigma, "verdict": verdict}
        return {"blocked": False, "sigma": sigma, "verdict": verdict}

    # --- Pipeline ---

    def check_input(self, text: str, *, max_input_tokens: float = 4096.0) -> Dict[str, Any]:
        for guard_fn in self.input_guards:
            name = getattr(guard_fn, "__name__", "")
            if name == "guard_length":
                result = self.guard_length(text, max_tokens=max_input_tokens)
            else:
                result = guard_fn(text)
            if result.get("blocked"):
                self.stats["blocked_input"] += 1
                return result
        return {"blocked": False}

    def check_output(self, prompt: str, response: str) -> Dict[str, Any]:
        sigma: Optional[float] = None
        verdict: Optional[str] = None
        for guard_fn in self.output_guards:
            name = getattr(guard_fn, "__name__", "")
            if name == "guard_sigma":
                result = self.guard_sigma(prompt, response)
            else:
                result = guard_fn(response)
            if result.get("blocked"):
                self.stats["blocked_output"] += 1
                return result
            if name == "guard_sigma":
                sigma = float(result["sigma"]) if result.get("sigma") is not None else None
                verdict = str(result.get("verdict")) if result.get("verdict") is not None else None
        self.stats["passed"] += 1
        return {"blocked": False, "sigma": sigma, "verdict": verdict}

    def full_pipeline(self, prompt: str, generate_fn: GenerateFn) -> Dict[str, Any]:
        input_check = self.check_input(prompt)
        if input_check.get("blocked"):
            return {"response": None, "blocked": True, "stage": "input", "reason": input_check}

        response = str(generate_fn(prompt))
        output_check = self.check_output(prompt, response)
        if output_check.get("blocked"):
            return {"response": None, "blocked": True, "stage": "output", "reason": output_check}

        return {
            "response": response,
            "blocked": False,
            "sigma": output_check.get("sigma"),
            "verdict": output_check.get("verdict"),
        }


def default_guardrails_stats_path() -> Path:
    d = Path.home() / ".cos"
    d.mkdir(parents=True, exist_ok=True)
    return d / "sigma_guardrails_stats.json"


def load_stats_into_guardrails(path: Path, gr: SigmaGuardrails) -> None:
    if not path.is_file():
        return
    try:
        blob = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return
    stats = blob.get("stats")
    if isinstance(stats, dict):
        for k in ("blocked_input", "blocked_output", "passed"):
            if k in stats:
                try:
                    gr.stats[k] = int(stats[k])
                except (TypeError, ValueError):
                    pass


def save_stats_from_guardrails(path: Path, gr: SigmaGuardrails) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps({"stats": dict(gr.stats)}, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


__all__ = [
    "SigmaGuardrails",
    "QuickscoreGate",
    "default_guardrails_stats_path",
    "load_stats_into_guardrails",
    "save_stats_from_guardrails",
]
