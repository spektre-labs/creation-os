# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-automotive — safety-critical voice lab: edge-only vehicle commands + manual RAG policy.

Regulatory and NCAP narratives belong in product comms with sources; this module is a **lab
orchestrator** (no telemetry, no certified latency). Pair with :mod:`cos.voice` and
:mod:`cos.rag`. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import re
import time
from typing import Any, Dict, List, Mapping, Optional, Sequence, Set

__all__ = ["SigmaAutomotive"]

_SAFETY_CRITICAL: Set[str] = {
    "brake",
    "lock",
    "unlock",
    "airbag",
    "diff_lock",
}


class SigmaAutomotive:
    """Hybrid edge/cloud policy stubs: vehicle commands never go to cloud LLM paths."""

    def __init__(self, gate: Any = None, *, chunk_doubt_threshold: float = 0.48) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.chunk_doubt_threshold = float(chunk_doubt_threshold)

    @property
    def safety_critical_commands(self) -> List[str]:
        return sorted(_SAFETY_CRITICAL)

    @property
    def offline_mode(self) -> bool:
        """σ-gate kernel is local/C-friendly; this Python mirror is still offline-capable (no I/O)."""
        return True

    @staticmethod
    def latency_budget() -> Dict[str, float]:
        """Design targets for integrators (not a certified bench)."""
        return {"edge_gate_target_ms": 50.0, "full_cascade_target_ms": 200.0}

    @staticmethod
    def _mentions_safety_command(text: str) -> bool:
        low = str(text).lower()
        if any(k in low for k in _SAFETY_CRITICAL):
            return True
        if re.search(r"\b(diff|differential)\s*lock\b", low):
            return True
        return False

    @staticmethod
    def _manual_relevance(query: str, text: str) -> float:
        q_words = set(re.findall(r"[a-z0-9]+", str(query).lower()))
        t_words = set(re.findall(r"[a-z0-9]+", str(text).lower()))
        stop = {"how", "do", "i", "the", "a", "an", "to", "is", "for", "on", "this", "that", "can"}
        q_words -= stop
        if not q_words:
            return 1.0
        inter = len(q_words & t_words)
        return inter / max(len(q_words), 1)

    def route_path(self, user_text: str) -> Dict[str, Any]:
        """If safety-critical intent appears, forbid cloud LLM / generative answers."""
        crit = self._mentions_safety_command(user_text)
        return {
            "edge_only": bool(crit),
            "cloud_llm_allowed": not crit,
            "safety_critical_detected": crit,
            "note": "Wire deterministic ECU/BCM paths for critical verbs; never free-form LLM.",
        }

    def enforce_vehicle_answer(
        self,
        query: str,
        draft_response: str,
        *,
        from_verified_manual: bool,
    ) -> Dict[str, Any]:
        """Block generative text for safety-critical topics unless tied to verified manual RAG."""
        if self._mentions_safety_command(query) and not from_verified_manual:
            return {
                "allow_voice": False,
                "verdict": "ABSTAIN",
                "sigma": 1.0,
                "reason": "safety_critical_requires_verified_manual",
            }
        sigma, verdict = self.gate.score(str(query), str(draft_response))
        return {
            "allow_voice": str(verdict).upper() == "ACCEPT",
            "verdict": str(verdict).upper(),
            "sigma": round(float(sigma), 6),
            "reason": "gate",
        }

    def manual_rag(
        self,
        query: str,
        vehicle_manual_chunks: Sequence[Mapping[str, Any]],
    ) -> Dict[str, Any]:
        """Score retrieved chunks; ABSTAIN with page hint when σ is high or source not verified."""
        best: Optional[Mapping[str, Any]] = None
        best_sigma = 2.0
        best_text = ""
        for ch in vehicle_manual_chunks:
            if not bool(ch.get("verified", False)):
                continue
            text = str(ch.get("text", "") or "")
            sigma, verdict = self.gate.score(f"vehicle_manual:{str(query)[:200]}", text[:4000])
            s = float(sigma)
            if s < best_sigma:
                best_sigma = s
                best = ch
                best_text = text
        page = int((best or {}).get("page", 0)) if best else 0
        if best is None:
            return {
                "verdict": "ABSTAIN",
                "sigma": 1.0,
                "spoken_hint": self._manual_fallback(page or 1),
                "source": "manual",
            }
        if best_sigma > self.chunk_doubt_threshold:
            return {
                "verdict": "ABSTAIN",
                "sigma": round(float(best_sigma), 6),
                "spoken_hint": f"See the owner manual around page {page} for this procedure.",
                "source": "manual_verified",
                "page": page,
            }
        rel = self._manual_relevance(str(query), best_text)
        if rel < 0.12:
            return {
                "verdict": "ABSTAIN",
                "sigma": round(float(best_sigma), 6),
                "spoken_hint": f"See the owner manual around page {page} for this procedure.",
                "source": "manual_low_relevance",
                "page": page,
            }
        sigma, verdict = self.gate.score(str(query), best_text[:2000])
        verdict_s = str(verdict).upper()
        if verdict_s == "ABSTAIN" or float(sigma) > 0.55:
            return {
                "verdict": "ABSTAIN",
                "sigma": round(float(sigma), 6),
                "spoken_hint": f"See the owner manual around page {page} for this procedure.",
                "source": "manual_verified",
                "page": page,
            }
        return {
            "verdict": verdict_s,
            "sigma": round(float(sigma), 6),
            "answer_excerpt": best_text[:800],
            "page": page,
            "source": "manual_verified",
        }

    @staticmethod
    def _manual_fallback(page: int) -> str:
        return f"I do not have a verified excerpt. Check the owner manual (page {page}) in the glove box."

    def context_aware_interrupt(
        self,
        driving_state: str,
        sigma: float,
        verdict: str,
    ) -> Dict[str, Any]:
        """Short-circuit long TTS when speed/attention demand it."""
        ds = str(driving_state).lower()
        v = str(verdict).upper()
        s = float(sigma)
        if ds in ("highway", "moving", "driving") or "120" in ds:
            max_words = 12 if v == "ACCEPT" else 0
        elif ds in ("traffic", "congestion", "crawl"):
            max_words = 25
        elif ds in ("parked", "standstill", "park"):
            max_words = 120
        else:
            max_words = 40
        interrupt = v in ("RETHINK", "ABSTAIN") or s > 0.6
        return {
            "max_words": max_words,
            "interrupt": interrupt,
            "allow_voice": max_words > 0 and not interrupt,
        }

    def voice_output_policy(
        self,
        sigma: float,
        verdict: str,
        driving_context: str,
    ) -> Dict[str, Any]:
        """Map σ + verdict + driving mode to short prompts for TTS (UI defers on RETHINK)."""
        v = str(verdict).upper()
        ctx = self.context_aware_interrupt(driving_context, sigma, verdict)
        if v == "RETHINK":
            return {
                "speak": "I am not certain. I will show details on the display when it is safe.",
                "defer_to_screen": True,
                "play_voice": False,
            }
        if v == "ABSTAIN":
            return {
                "speak": "I do not know that. Please use the owner manual in the glove box.",
                "defer_to_screen": False,
                "play_voice": True,
            }
        if v == "ACCEPT" and ctx["max_words"] <= 15:
            return {
                "speak": "Short confirmation only while driving.",
                "defer_to_screen": False,
                "play_voice": True,
            }
        return {
            "speak": "Here is the verified answer from your manual.",
            "defer_to_screen": False,
            "play_voice": True,
        }

    def gate_latency_probe_ms(self, *, iterations: int = 8) -> float:
        """Average ``gate.score`` time (Python path; not the C fixed-point kernel)."""
        n = max(1, int(iterations))
        t0 = time.perf_counter()
        for _ in range(n):
            self.gate.score("latency_probe", "ack")
        return (time.perf_counter() - t0) / n * 1000.0
