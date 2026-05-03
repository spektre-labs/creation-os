# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-serving — lab hooks to pair σ-gate scoring with inference stacks (vLLM, SGLang, etc.).

Continuous batching, prefix caching, and schedulers here are **scaffolding**: wire real
telemetry from your driver. Do not cite external throughput multipliers as measured in-tree;
see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import threading
import time
from collections import deque
from typing import Any, Callable, Deque, Dict, List, Mapping, Optional, Sequence, Tuple

__all__ = ["SigmaServing", "ThroughputMonitor"]


class ThroughputMonitor:
    """Rolling window of token scores and σ evaluations (wall clock, single process)."""

    def __init__(self, window_seconds: float = 60.0) -> None:
        self.window_seconds = float(max(0.5, window_seconds))
        self._lock = threading.Lock()
        self._events: Deque[Tuple[float, int, int]] = deque()  # (t, n_tokens, n_sigma)

    def record(self, *, tokens: int = 0, sigma_evals: int = 1) -> None:
        now = time.monotonic()
        with self._lock:
            self._events.append((now, int(tokens), int(sigma_evals)))
            self._trim_unlocked(now)

    def _trim_unlocked(self, now: float) -> None:
        cut = now - self.window_seconds
        while self._events and self._events[0][0] < cut:
            self._events.popleft()

    def snapshot(self) -> Dict[str, float]:
        now = time.monotonic()
        with self._lock:
            self._trim_unlocked(now)
            dt = self.window_seconds
            if self._events:
                span = max(now - self._events[0][0], 1e-6)
                dt = min(self.window_seconds, span)
            tok = sum(e[1] for e in self._events)
            sig = sum(e[2] for e in self._events)
        return {
            "window_s": round(float(self.window_seconds), 3),
            "tokens_per_s": round(tok / dt, 6),
            "sigma_evals_per_s": round(sig / dt, 6),
            "events": float(len(self._events)),
        }


class SigmaServing:
    """Callbacks and batching helpers; ``cos serve`` uses :meth:`batch_score` for `/v1/batch`."""

    def __init__(self) -> None:
        self._prefix_sigma: Dict[str, Tuple[float, str]] = {}
        self._prefix_lock = threading.Lock()
        self._monitors: Dict[str, ThroughputMonitor] = {}

    def throughput_monitor(self, window_seconds: float = 60.0, *, name: str = "default") -> ThroughputMonitor:
        w = float(window_seconds)
        key = f"{name}:{w}"
        if key not in self._monitors:
            self._monitors[key] = ThroughputMonitor(w)
        return self._monitors[key]

    @staticmethod
    def vllm_hook(model: Any, gate: Any) -> Dict[str, Any]:
        """Return a callable integrators can run after each vLLM generation (``model`` unused lab)."""

        def after_generation(prompt: str, response: str) -> Tuple[float, str]:
            sigma, verdict = gate.score(str(prompt), str(response))
            return float(sigma), str(verdict)

        return {
            "framework": "vllm",
            "after_generation": after_generation,
            "note": "Attach after_generation to your engine output hook; ``model`` is for future wiring.",
        }

    @staticmethod
    def sglang_hook(model: Any, gate: Any) -> Dict[str, Any]:
        """Return a post-commit hook shape for SGLang-style runtimes (``model`` reserved)."""

        def after_emit(prompt: str, response: str) -> Tuple[float, str]:
            sigma, verdict = gate.score(str(prompt), str(response))
            return float(sigma), str(verdict)

        return {
            "framework": "sglang",
            "after_emit": after_emit,
            "note": "Call after_emit when a program finishes emitting text for a request.",
        }

    def batch_score(
        self,
        requests: Sequence[Mapping[str, Any]],
        gate: Any,
        *,
        tokens_per_item: int = 0,
        monitor: Optional[ThroughputMonitor] = None,
    ) -> List[Dict[str, Any]]:
        """Score many prompt/response rows (continuous-batch friendly; order preserved)."""
        mon = monitor
        out: List[Dict[str, Any]] = []
        for row in requests:
            p = str(row.get("prompt", ""))
            r = str(row.get("response", ""))
            sigma, verdict = gate.score(p, r)
            rec: Dict[str, Any] = {
                "prompt": p,
                "response": r,
                "sigma": round(float(sigma), 6),
                "verdict": str(verdict),
            }
            out.append(rec)
            if mon is not None:
                mon.record(tokens=int(tokens_per_item), sigma_evals=1)
        return out

    def prefix_cache_sigma(self, prefix: str, gate: Any, *, response_stub: str = "") -> Dict[str, Any]:
        """Cache (σ, verdict) keyed by stable hash of ``prefix`` (optional stub completion)."""
        key = hashlib.sha256(str(prefix).encode()).hexdigest()[:24]
        stub = str(response_stub) if response_stub else "[prefill]"
        with self._prefix_lock:
            if key in self._prefix_sigma:
                s, v = self._prefix_sigma[key]
                return {"cache_hit": True, "sigma": s, "verdict": v, "key": key}
        sigma, verdict = gate.score(str(prefix), stub)
        with self._prefix_lock:
            self._prefix_sigma[key] = (round(float(sigma), 6), str(verdict))
        return {"cache_hit": False, "sigma": float(sigma), "verdict": str(verdict), "key": key}

    @staticmethod
    def sigma_aware_scheduling(
        requests: Sequence[Mapping[str, Any]],
        gate: Any,
        *,
        cascade_fn: Optional[Callable[[str, str], Mapping[str, Any]]] = None,
        sigma_fast_threshold: float = 0.35,
    ) -> Dict[str, Any]:
        """Partition rows into *fast* (low σ, L1-style) vs *deep* (cascade / verify) paths."""
        fast: List[Dict[str, Any]] = []
        deep: List[Dict[str, Any]] = []
        for row in requests:
            p, r = str(row.get("prompt", "")), str(row.get("response", ""))
            sigma, verdict = gate.score(p, r)
            item = {"prompt": p, "response": r, "sigma": float(sigma), "verdict": str(verdict)}
            if float(sigma) <= float(sigma_fast_threshold):
                fast.append(item)
            else:
                if cascade_fn is not None:
                    item["cascade"] = dict(cascade_fn(p, r))
                deep.append(item)
        return {"fast_path": fast, "deep_path": deep, "n_fast": len(fast), "n_deep": len(deep)}
