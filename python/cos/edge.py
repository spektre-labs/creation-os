# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""Edge-oriented hints for running the sigma-gate and optional Python probes by device tier.

The portable measurement core is ``sigma_gate.h`` (12-byte C89 wire shape); this module only
summarizes **heuristic** RAM/tier labels and probe depth — not a hard real-time guarantee.
See ``docs/CLAIM_DISCIPLINE.md``. **NOT AGI.**
"""
from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

from cos.probes import L1EntropyProbe, L2LogProbVariance, SigmaFusion
from cos.sigma_gate import SigmaGate


def _get_ram_mb() -> int:
    """Best-effort physical RAM in MiB (may fall back to a conservative default)."""
    try:
        if sys.platform == "darwin":
            sysctl_bin = shutil.which("sysctl") or "/usr/sbin/sysctl"
            out = subprocess.check_output([sysctl_bin, "-n", "hw.memsize"], text=True, timeout=2)
            bts = int(out.strip())
            return max(1, bts // (1024 * 1024))
        meminfo = Path("/proc/meminfo")
        if meminfo.is_file():
            for line in meminfo.read_text(encoding="utf-8", errors="replace").splitlines():
                if line.startswith("MemTotal:"):
                    kb = int(line.split()[1])
                    return max(1, kb // 1024)
        import os

        if hasattr(os, "sysconf"):
            pages = int(os.sysconf("SC_PHYS_PAGES"))
            page_size = int(os.sysconf("SC_PAGE_SIZE"))
            return max(1, (pages * page_size) // (1024 * 1024))
    except Exception:
        pass
    return 4096


def _tier_config(tier: int) -> Dict[str, Any]:
    """Recommended lab-style settings per tier (documentation / orchestration hints)."""
    if tier <= 1:
        return {
            "model": "none (sigma_gate C core only)",
            "probes": ["L1"],
            "max_tokens": 64,
            "cascade_depth": 1,
            "quantization": "INT4",
            "kv_cache": 128,
            "note": "Tier 1: smallest footprint; flash C89 kernel to MCU class hardware.",
        }
    if tier == 2:
        return {
            "model": "SmolLM2-135M or Qwen2.5-0.5B (local Q4_K_M class)",
            "probes": ["L1", "L2"],
            "max_tokens": 256,
            "cascade_depth": 2,
            "quantization": "Q4_K_M",
            "kv_cache": 512,
            "note": "Tier 2: phone / SBC class; Python probes + local inference, no cloud required.",
        }
    return {
        "model": "Multi-B gate/host models (when RAM allows)",
        "probes": ["L1", "L2", "L3", "L4", "L5"],
        "max_tokens": 4096,
        "cascade_depth": 5,
        "quantization": "Q4_K_M to Q6_K",
        "kv_cache": 4096,
        "note": "Tier 3: laptop / server; full Python fusion stack (still zero-dep for probe fallbacks).",
    }


class EdgeProfile:
    """Summarize host capabilities into coarse deployment tiers."""

    @staticmethod
    def detect() -> Dict[str, Any]:
        import platform

        total_ram = _get_ram_mb()
        if total_ram < 256:
            tier = 1
        elif total_ram < 2048:
            tier = 2
        else:
            tier = 3
        return {
            "tier": tier,
            "ram_mb": total_ram,
            "cpu": platform.machine(),
            "platform": platform.system(),
            "python": f"{sys.version_info.major}.{sys.version_info.minor}",
            "recommended": _tier_config(tier),
        }


class EdgeSigmaGate:
    """Score (prompt, response) with probe depth chosen by tier (L1 only / L1+L2 / full fusion)."""

    def __init__(self, tier: Optional[int] = None) -> None:
        prof = EdgeProfile.detect()
        self.tier = int(tier if tier is not None else prof["tier"])
        self.config = _tier_config(self.tier)
        self._gate = SigmaGate()
        self._l1 = L1EntropyProbe()
        self._l2 = L2LogProbVariance()
        self._fusion: Optional[SigmaFusion] = None

    def _verdict(self, sigma: float) -> str:
        if sigma < 0.2:
            return "ACCEPT"
        if sigma < 0.5:
            return "RETHINK"
        return "ABSTAIN"

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        if self.tier <= 1:
            s = float(self._l1.score(prompt, response))
        elif self.tier == 2:
            s1 = float(self._l1.score(prompt, response))
            s2 = float(self._l2.score(prompt, response))
            s = (s1 + s2) / 2.0
        else:
            if self._fusion is None:
                self._fusion = SigmaFusion()
            s, _, _ = self._fusion.score(prompt, response)
            s = float(s)
        s = round(min(1.0, max(0.0, s)), 4)
        return s, self._verdict(s)

    def can_run(self, model_size_b: float) -> Dict[str, Any]:
        """Heuristic: Q4-class ~0.6 GiB per billion parameters (lab rule of thumb)."""
        ram = int(EdgeProfile.detect()["ram_mb"])
        need = int(float(model_size_b) * 600.0)
        fits = ram > need
        approx_b = max(ram // 600, 0) if ram >= 600 else 0
        rec = (
            f"fits in RAM ({ram} MiB > {need} MiB)"
            if fits
            else f"too large (need ~{need} MiB, have {ram} MiB) — try ~{approx_b}B or smaller Q4"
        )
        return {
            "can_run": fits,
            "ram_mb": ram,
            "needed_mb": need,
            "model_size_b": float(model_size_b),
            "recommendation": rec,
        }


__all__ = ["EdgeProfile", "EdgeSigmaGate", "_get_ram_mb", "_tier_config"]
