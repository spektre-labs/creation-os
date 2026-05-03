# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-edge — on-device SLM deployment **profiles** (lab).

TOPS / backend claims are **not** benchmarked here; profiles are operator-facing caps.
Wire ExecuTorch / llama.cpp / NPU SDKs outside this module. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, Mapping, Optional

__all__ = ["SigmaEdge"]


class SigmaEdge:
    """Device budgets, quantization hints, TTC schedule, battery-aware cascade skipping."""

    @staticmethod
    def deploy_profile(device_type: str) -> Dict[str, Any]:
        dt = str(device_type).lower().strip()
        profiles = {
            "phone": {
                "max_model_mb": 500,
                "max_latency_ms": 100,
                "battery_aware": True,
                "offline": False,
            },
            "laptop": {
                "max_model_mb": 4000,
                "max_latency_ms": 200,
                "battery_aware": False,
                "offline": False,
            },
            "mcu": {
                "max_model_mb": 2,
                "max_latency_ms": 50,
                "battery_aware": True,
                "offline": True,
                "llm": False,
                "note": "σ-gate-only footprint path; pair with ``cos.tiny`` style C core.",
            },
            "automotive": {
                "max_model_mb": 2000,
                "max_latency_ms": 50,
                "battery_aware": False,
                "offline": True,
            },
        }
        if dt not in profiles:
            return {**profiles["laptop"], "device_type": dt, "custom": True}
        return {**profiles[dt], "device_type": dt}

    @staticmethod
    def model_select(task: str, device_profile: Mapping[str, Any]) -> Dict[str, Any]:
        """Return a **label** pick (no download). Names are common SLM families."""
        t = str(task).lower()
        cap_mb = float(device_profile.get("max_model_mb", 4000))
        picks = [
            ("code" in t or "sql" in t, "phi-4-mini"),
            ("qwen" in t or "zh" in t, "qwen2.5-0.5b-instruct"),
            ("reason" in t or "math" in t, "llama-3.2-1b-instruct"),
            (cap_mb <= 400, "gemma-2-270m-it"),
            (True, "llama-3.2-1b-instruct"),
        ]
        for cond, name in picks:
            if cond:
                return {"model": name, "rationale": "heuristic_keyword_and_cap", "cap_mb": cap_mb}
        return {"model": "unknown", "rationale": "fallback", "cap_mb": cap_mb}

    @staticmethod
    def quantize_for_device(_model: Any, device_profile: Mapping[str, Any]) -> Dict[str, Any]:
        del _model
        dt = str(device_profile.get("device_type", "laptop")).lower()
        if dt == "phone":
            q = "Q4_K_M"
        elif dt == "mcu":
            q = "Q2_K"
        elif dt == "laptop":
            q = "Q8_0"
        else:
            q = "Q4_K_M"
        return {
            "quantization": q,
            "device_type": dt,
            "disclaimer": "GGUF-style label for planning; run real quants with your toolchain.",
        }

    @staticmethod
    def sigma_gate_overhead(device: str) -> Dict[str, Any]:
        """Planning numbers + **disclaimer** — not a profiler output."""
        d = str(device).lower()
        return {
            "device": d,
            "c_core_sigma_gate_ms": "<1 (planning — C89 path separate binary)",
            "probe_l1_l3_ms": "<10 (planning — edge SoC dependent)",
            "probe_l4_l5": "cloud_or_skip",
            "disclaimer": "Measure on-target; do not cite planning rows as benchmarks.",
        }

    def test_time_compute(
        self,
        query: str,
        model: Any,
        gate: Any,
        *,
        difficulty: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Easy → one draft; hard → N drafts + pick lowest σ (lab orchestration)."""
        diff = str(difficulty or self._difficulty_heuristic(query)).lower()
        n = 1 if diff == "easy" else max(2, min(8, len(query) // 80 + 2))
        gen = getattr(model, "generate", None)
        candidates: list[tuple[float, str]] = []
        if not callable(gen):
            text = str(model)
            sigma, verdict = gate.score(str(query), text)
            return {"n": 1, "chosen": text, "sigma": float(sigma), "verdict": str(verdict), "difficulty": diff}
        for i in range(n):
            cand = str(gen(f"{query} [ttc:{i}]"))
            sigma, _ = gate.score(str(query), cand)
            candidates.append((float(sigma), cand))
        candidates.sort(key=lambda x: x[0])
        best_s, best_t = candidates[0]
        _, verdict = gate.score(str(query), best_t)
        return {
            "n": n,
            "chosen": best_t,
            "sigma": best_s,
            "verdict": str(verdict),
            "difficulty": diff,
            "candidates": len(candidates),
        }

    @staticmethod
    def _difficulty_heuristic(query: str) -> str:
        q = str(query).strip()
        if len(q) < 40 and q.count(" ") < 8:
            return "easy"
        if len(q) > 400 or q.count("?") > 2:
            return "hard"
        return "medium"

    @staticmethod
    def battery_budget(remaining_percent: float, queries_remaining: int) -> Dict[str, Any]:
        rp = max(0.0, min(100.0, float(remaining_percent)))
        qr = max(0, int(queries_remaining))
        save = rp < 20.0
        return {
            "remaining_percent": rp,
            "queries_remaining": qr,
            "skip_probe_levels_l2_l5": save,
            "lower_max_ttc_branches": save,
            "note": "Policy hook for fleet policy engines — tune thresholds per OEM.",
        }
