# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-prefill — prefill/decode disaggregation sketches and pre-generation σ checks (lab).

Does not drive real two-stage serving; integrate with ``cos.serving`` and KV paths.
See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import statistics
from typing import Any, Dict, List, Mapping, Optional

__all__ = ["SigmaPrefill"]


class SigmaPrefill:
    """Chunking, assignment dicts, and cheap σ-before-decode heuristics."""

    def __init__(self) -> None:
        self._prefix_cache: Dict[str, Dict[str, Any]] = {}

    @staticmethod
    def disaggregate(request: Mapping[str, Any]) -> Dict[str, Any]:
        """Label prefill vs decode workers (placeholders for GPU ids / queue names)."""
        return {
            "prefill_gpu": str(request.get("prefill_gpu", "prefill:0")),
            "decode_gpu": str(request.get("decode_gpu", "decode:0")),
            "disaggregated": True,
        }

    @staticmethod
    def chunked_prefill(prompt: str, chunk_size: int) -> List[str]:
        """Split a long prompt into chunks (character-wise; swap for tokenizer in prod)."""
        p = str(prompt)
        k = max(1, int(chunk_size))
        return [p[i : i + k] for i in range(0, len(p), k)] or [""]

    def sigma_after_prefill(
        self,
        hidden_states: Any,
        *,
        gate: Optional[Any] = None,
        prompt: str = "",
    ) -> Dict[str, Any]:
        """High σ before decode ⇒ run cascade / verify; low σ ⇒ cheap path.

        With only numeric hidden summaries, uses dispersion proxy; if ``gate`` and ``prompt`` are
        set, blends in a lightweight text σ on ``prompt`` + stub completion.
        """
        proxy = self._hidden_dispersion(hidden_states)
        text_sigma: Optional[float] = None
        gate_verdict: Optional[str] = None
        if gate is not None and prompt:
            s, v = gate.score(str(prompt), "[prefill_complete]")
            text_sigma = float(s)
            gate_verdict = str(v)
            combined = 0.5 * proxy + 0.5 * text_sigma
        else:
            combined = proxy
        verdict_hint = "deep_path" if combined > 0.45 else "fast_path"
        return {
            "sigma_proxy": round(float(proxy), 6),
            "sigma_text": text_sigma,
            "sigma_combined": round(float(combined), 6),
            "verdict_hint": verdict_hint,
            "gate_verdict": gate_verdict,
        }

    @staticmethod
    def _hidden_dispersion(hidden_states: Any) -> float:
        if hidden_states is None:
            return 0.5
        if hasattr(hidden_states, "shape"):
            return 0.55
        flat: List[float] = []
        for layer in hidden_states:
            if layer is None:
                continue
            if isinstance(layer, (list, tuple)):
                for row in layer:
                    if isinstance(row, (list, tuple)):
                        flat.extend(float(x) for x in row)
                    else:
                        flat.append(float(row))
            else:
                flat.append(float(layer))  # type: ignore[arg-type]
        if len(flat) < 2:
            return 0.5
        return float(min(1.0, statistics.pstdev(flat)))

    def prefill_cache(self, prefix: str, gate: Any) -> Dict[str, Any]:
        """Cache σ for a prefix (shared with serving-style prefix reuse)."""
        key = hashlib.sha256(str(prefix).encode()).hexdigest()[:20]
        if key in self._prefix_cache:
            return {"hit": True, **self._prefix_cache[key]}
        sigma, verdict = gate.score(str(prefix), "[cached_prefill]")
        row = {"sigma": round(float(sigma), 6), "verdict": str(verdict)}
        self._prefix_cache[key] = row
        return {"hit": False, **row, "key": key}

    @staticmethod
    def ttft_optimization(*, max_gate_ms: float = 5.0, use_prefix_cache: bool = True) -> Dict[str, Any]:
        """Guidance: keep σ evaluations under ``max_gate_ms`` on the prefill hot path."""
        return {
            "max_gate_ms": float(max_gate_ms),
            "use_prefix_cache": bool(use_prefix_cache),
            "prefer": "L1-only or cached σ during prefill; defer cascade to decode if TTFT budget slips.",
        }
