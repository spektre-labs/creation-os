# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""End-to-end σ pipeline: prompt → guardrails → model → σ-gate → decision → output.

Pure Python; core install has no extra dependencies. Wire a ``model`` with a
``.generate(prompt, **kwargs) -> str`` method for full generate+score loops.
"""
from __future__ import annotations

import re
from typing import Any, Dict, Optional

from cos.probe import SignalCascade
from cos.sigma_gate import SigmaGate


class PipelineResult:
    """One pipeline step outcome: text + σ + verdict (+ optional ``reason``)."""

    __slots__ = ("text", "sigma", "verdict", "reason", "attempt")

    def __init__(
        self,
        text: Optional[str],
        sigma: float,
        verdict: str,
        *,
        reason: Optional[str] = None,
        attempt: int = 1,
    ) -> None:
        self.text = text
        self.sigma = float(sigma)
        self.verdict = str(verdict)
        self.reason = reason
        self.attempt = int(attempt)

    def __str__(self) -> str:
        return self.text or ""

    def __repr__(self) -> str:
        clip = (self.text or "")[:50] + ("…" if self.text and len(self.text) > 50 else "")
        return f"PipelineResult(σ={self.sigma:.3f}, {self.verdict!r}, {clip!r})"

    def __bool__(self) -> bool:
        return self.verdict == "ACCEPT"

    def to_dict(self) -> Dict[str, Any]:
        return {
            "text": self.text,
            "sigma": self.sigma,
            "verdict": self.verdict,
            "reason": self.reason,
            "attempt": self.attempt,
        }


class PipelineConfig:
    def __init__(
        self,
        max_retries: int = 2,
        max_input_tokens: int = 4096,
        max_probe_tokens: int = 2048,
    ) -> None:
        self.max_retries = int(max_retries)
        self.max_input_tokens = int(max_input_tokens)
        self.max_probe_tokens = int(max_probe_tokens)


class PipelineStats:
    def __init__(self) -> None:
        self.total = 0
        self.accept = 0
        self.rethink = 0
        self.abstain = 0
        self.blocked = 0

    def update(self, result: PipelineResult) -> None:
        self.total += 1
        v = result.verdict.lower()
        if v == "accept":
            self.accept += 1
        elif v == "rethink":
            self.rethink += 1
        elif v == "abstain":
            self.abstain += 1

    def to_dict(self) -> Dict[str, Any]:
        return {
            "total": self.total,
            "accept": self.accept,
            "rethink": self.rethink,
            "abstain": self.abstain,
            "blocked": self.blocked,
            "accept_rate": self.accept / max(self.total, 1),
        }


_INJECTION_PATTERNS = tuple(
    re.compile(p, re.IGNORECASE)
    for p in (
        r"ignore\s+(all\s+)?previous\s+instructions",
        r"you\s+are\s+now\s+",
        r"disregard\s+(all|your)\s+",
        r"system\s+prompt",
        r"\[INST\]",
        r"<\|im_start\|>",
    )
)

_PII_PATTERNS = {
    "ssn": re.compile(r"\b\d{3}-\d{2}-\d{4}\b"),
    "credit_card": re.compile(r"\b\d{4}[-\s]?\d{4}[-\s]?\d{4}[-\s]?\d{4}\b"),
}


class Pipeline:
    """σ-gated inference orchestrator (score-only by default; optional ``model``)."""

    def __init__(
        self,
        model: Any = None,
        gate: Any = None,
        config: Optional[PipelineConfig] = None,
        probe: Any = None,
        tokenizer: Any = None,
        calibrator: Any = None,
        metacog: Any = None,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.model = model
        self.config = config or PipelineConfig()
        self.stats = PipelineStats()
        self.probe = probe
        self.tokenizer = tokenizer
        self.cascade = SignalCascade()
        self.calibrator = calibrator
        self.metacog = metacog

    def run(self, prompt: str, **kwargs: Any) -> PipelineResult:
        # 1. INPUT CHECK
        input_check = self.check_input(prompt)
        if input_check["blocked"]:
            self.stats.blocked += 1
            return PipelineResult(
                text=None,
                sigma=1.0,
                verdict="BLOCKED",
                reason=str(input_check.get("reason", "blocked")),
            )

        best: Optional[PipelineResult] = None
        max_try = self.config.max_retries + 1
        _SKIP_GEN = frozenset(("response", "metacog_signals", "signals"))
        gen_kwargs = {k: v for k, v in kwargs.items() if k not in _SKIP_GEN}

        for attempt in range(max_try):
            if self.model is not None:
                response = self.model.generate(prompt, **gen_kwargs)
            elif "response" in kwargs:
                response = kwargs["response"]
            else:
                return PipelineResult(
                    text=None,
                    sigma=1.0,
                    verdict="NO_MODEL",
                    reason=(
                        "No model configured. Use pipe.run(prompt, response='...') or pass model=."
                    ),
                )

            sigma, verdict = self._score_pair(str(prompt), str(response))
            sigma = float(sigma)
            if self.calibrator is not None:
                sigma = float(self.calibrator.calibrate(sigma))
                verdict = str(self.gate._verdict(sigma))
            result = PipelineResult(
                text=str(response),
                sigma=float(sigma),
                verdict=str(verdict),
                attempt=attempt + 1,
            )

            if best is None or result.sigma < best.sigma:
                best = result

            if verdict == "ACCEPT":
                break
            if verdict == "RETHINK" and attempt < self.config.max_retries:
                continue
            break

        assert best is not None

        if best.verdict == "ACCEPT" and best.text:
            output_check = self.check_output(best.text)
            if output_check["blocked"]:
                self.stats.blocked += 1
                best = PipelineResult(
                    text=None,
                    sigma=1.0,
                    verdict="OUTPUT_BLOCKED",
                    reason=str(output_check.get("reason", "output blocked")),
                    attempt=best.attempt,
                )

        if best.verdict == "ABSTAIN":
            best = PipelineResult(
                text="I don't have a reliable answer for this.",
                sigma=best.sigma,
                verdict="ABSTAIN",
                attempt=best.attempt,
            )

        best = self._apply_metacog(str(prompt), best, kwargs)

        self.stats.update(best)
        return best

    def score(self, prompt: str, response: str) -> PipelineResult:
        """Score-only: no generation; same as ``run(..., response=...)``."""
        return self.run(prompt, response=response)

    def _score_pair(self, prompt: str, response: str) -> tuple[float, str]:
        """Score with cascade (probe+model) when wired; else ΣGate."""
        if self.probe is not None and self.model is not None and self.tokenizer is not None:
            try:
                text = f"{prompt}{response}"
                enc = self.tokenizer(
                    text,
                    return_tensors="pt",
                    truncation=True,
                    max_length=int(self.config.max_probe_tokens),
                )
                input_ids = enc["input_ids"]
                attention_mask = enc.get("attention_mask")
                if getattr(self.probe, "model", None) is None:
                    self.probe.model = self.model
                states = self.probe.extract(input_ids, attention_mask=attention_mask)
                cascade_result = self.cascade.score(
                    prompt,
                    response,
                    hidden_states=states.get("hidden_states"),
                    logits=states.get("logits"),
                )
                return float(cascade_result["sigma"]), str(cascade_result["verdict"])
            except Exception:
                pass
        s, v = self.gate.score(prompt, response)
        return float(s), str(v)

    def _apply_metacog(
        self,
        prompt: str,
        result: PipelineResult,
        kwargs: Dict[str, Any],
    ) -> PipelineResult:
        if self.metacog is None:
            return result
        if result.text is None or result.verdict in ("BLOCKED", "NO_MODEL", "OUTPUT_BLOCKED"):
            return result

        signals = kwargs.get("metacog_signals")
        if signals is None:
            signals = kwargs.get("signals")
        if not isinstance(signals, dict):
            signals = {}

        assessment = self.metacog.assess(prompt, result.text, result.sigma, signals)
        action = str(assessment.get("action", "respond"))

        if action == "abstain":
            return PipelineResult(
                text="I don't have a reliable answer for this.",
                sigma=max(float(result.sigma), float(assessment.get("total_sigma", 0.0))),
                verdict="ABSTAIN",
                reason="metacog_abstain",
                attempt=result.attempt,
            )

        if action == "clarify":
            return PipelineResult(
                text=result.text,
                sigma=float(result.sigma),
                verdict=result.verdict,
                reason="ambiguous_input",
                attempt=result.attempt,
            )

        if action == "retrieve":
            return PipelineResult(
                text=result.text,
                sigma=float(result.sigma),
                verdict=result.verdict,
                reason=(result.reason or "metacog_retrieve"),
                attempt=result.attempt,
            )

        if action == "clarify_and_retrieve":
            return PipelineResult(
                text=result.text,
                sigma=float(result.sigma),
                verdict=result.verdict,
                reason="ambiguous_input;metacog_retrieve",
                attempt=result.attempt,
            )

        return result

    def check_input(self, text: str) -> Dict[str, Any]:
        if not text or not str(text).strip():
            return {"blocked": True, "reason": "empty input"}

        words = len(str(text).split())
        if words > self.config.max_input_tokens:
            return {"blocked": True, "reason": "input too long"}

        lowered = str(text).lower()
        for rx in _INJECTION_PATTERNS:
            if rx.search(lowered):
                return {"blocked": True, "reason": f"injection matched: {rx.pattern!r}"}

        return {"blocked": False}

    def check_output(self, text: str) -> Dict[str, Any]:
        for name, rx in _PII_PATTERNS.items():
            if rx.search(text):
                return {"blocked": True, "reason": f"PII leak: {name}"}
        return {"blocked": False}


__all__ = [
    "Pipeline",
    "PipelineConfig",
    "PipelineResult",
    "PipelineStats",
]
