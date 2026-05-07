# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Host hardware introspection and **conservative** local inference hints (lab).

RAM detection uses **psutil** when installed; otherwise ``ram_gb`` may be ``0``.
Override for scripts: environment variable ``COS_RAM_GB`` (integer GB).

This module does **not** run benchmarks; see ``docs/HARDWARE_SETUP.md`` and
``docs/CLAIM_DISCIPLINE.md`` for evidence hygiene on tokens/s claims."""
from __future__ import annotations

import os
import platform
import subprocess
from typing import Any, Dict

__all__ = ["HardwareInfo"]


class HardwareInfo:
    """Detect CPU/OS, optional RAM, coarse GPU class, and heuristic recommendations."""

    def detect(self) -> Dict[str, Any]:
        info: Dict[str, Any] = {
            "platform": platform.system(),
            "machine": platform.machine(),
            "cpu": platform.processor() or "",
            "ram_gb": self._ram_gb(),
            "gpu": self._detect_gpu(),
            "apple_silicon": self._is_apple_silicon(),
        }
        info["recommendation"] = self._recommend(info)
        return info

    def _ram_gb(self) -> int:
        env = (os.environ.get("COS_RAM_GB") or "").strip()
        if env:
            try:
                return max(0, int(float(env)))
            except ValueError:
                pass
        try:
            import psutil  # type: ignore[import-untyped]

            return int(round(psutil.virtual_memory().total / (1024**3)))
        except ImportError:
            return 0

    def _detect_gpu(self) -> Dict[str, Any]:
        try:
            result = subprocess.run(
                [
                    "nvidia-smi",
                    "--query-gpu=name,memory.total",
                    "--format=csv,noheader",
                ],
                capture_output=True,
                text=True,
                timeout=5,
                check=False,
            )
            if result.returncode == 0 and (result.stdout or "").strip():
                return {"type": "nvidia", "info": (result.stdout or "").strip()}
        except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
            pass

        if self._is_apple_silicon():
            proc = (platform.processor() or "").strip()
            return {
                "type": "apple_silicon",
                "info": proc or "Apple Silicon (arm64)",
            }

        return {"type": "cpu_only", "info": (platform.processor() or "cpu").strip()}

    def _is_apple_silicon(self) -> bool:
        return platform.system() == "Darwin" and platform.machine() == "arm64"

    def _recommend(self, info: Dict[str, Any]) -> Dict[str, Any]:
        ram = int(info.get("ram_gb") or 0)
        gpu_t = (info.get("gpu") or {}).get("type")

        if gpu_t == "apple_silicon":
            if ram >= 128:
                return {
                    "model": "70B-class GGUF Q6_K or similar (if your runtime supports it)",
                    "backend": "MLX-LM (often fastest) or llama.cpp with flash-attn when available",
                    "expected_speed": "varies widely; profile locally",
                }
            if ram >= 64:
                return {
                    "model": "Qwen-family ~27B-class Q5_K_M or Q4_K_M (examples only)",
                    "backend": "MLX-LM or llama.cpp (OpenAI-compatible server)",
                    "expected_speed": "order-of-tens tokens/s (machine-dependent)",
                }
            if ram >= 24:
                return {
                    "model": "MoE or ~A3B active footprint Q4_K_M (examples only)",
                    "backend": "llama.cpp or MLX-LM",
                    "expected_speed": "often higher tokens/s than dense 27B on same chip (profile locally)",
                }
            return {
                "model": "~8B-class Q4_K_M as a conservative default",
                "backend": "llama.cpp",
                "expected_speed": "low-to-mid tens tokens/s (profile locally)",
            }

        if gpu_t == "nvidia":
            return {
                "model": "VRAM-sized quant (often Q4_K_M+) or vLLM-served HF weights",
                "backend": "vLLM (typical production path) or llama.cpp / TensorRT-LLM stacks",
                "expected_speed": "dominated by GPU VRAM and batching; profile locally",
            }

        return {
            "model": "~7B–8B-class Q4_K_M for CPU smoke tests",
            "backend": "llama.cpp CPU",
            "expected_speed": "single-digit to low-double-digit tokens/s typical on desktop CPUs",
        }
