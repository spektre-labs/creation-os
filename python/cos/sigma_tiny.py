# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""v160 σ-tiny lab: TinyML paths, footprint JSON, sensor σ demo (Python mirror)."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

from pathlib import Path
from typing import Any, Dict

REPO_ROOT = Path(__file__).resolve().parents[2]
TINY_DIR = REPO_ROOT / "hw" / "tinyml"


def tiny_generate_info(target: str) -> Dict[str, Any]:
    t = (target or "esp32").strip().lower()
    paths: Dict[str, str] = {
        "sigma_gate_tiny_h": str(TINY_DIR / "sigma_gate_tiny.h"),
        "sigma_sensor_c": str(TINY_DIR / "sigma_sensor.c"),
        "sigma_sensor_h": str(TINY_DIR / "sigma_sensor.h"),
        "footprint_h": str(TINY_DIR / "sigma_footprint.h"),
    }
    if t in ("esp32", "esp32s3", "esp32c3"):
        paths["sketch_ino"] = str(TINY_DIR / "sigma_gate_esp32.ino")
    elif t in ("attiny85", "avr", "attiny"):
        paths["mcu_template_c"] = str(TINY_DIR / "sigma_gate_attiny85.c")
    else:
        paths["note"] = f"unknown target {t!r}; see hw/tinyml/ for sources"
    return {
        "generated": False,
        "target": t,
        "note": "Golden sources live under hw/tinyml/ (copy headers next to your Arduino sketch if needed).",
        "paths": paths,
    }


def tiny_footprint_json() -> Dict[str, Any]:
    return {
        "evidence_class": "structure + order-of-magnitude (not audited per-ISA ROM)",
        "sigma_state_ram_bytes": 12,
        "sigma_sensor_total_ram_bytes_typical": 28,
        "rom_note": "Gate-only ROM is toolchain-dependent (typically hundreds of bytes); full firmware dominates.",
        "fits_constrained_mcus": [
            "ATtiny-class (RAM budget permitting the rest of the app)",
            "STM32F0 / nRF52 / ESP8266 / ESP32",
        ],
        "compare_note": "INT8 graph runtimes (TFLM, Edge Impulse, …) usually occupy **orders of magnitude** more RAM/flash than this σ primitive alone.",
        "paths": {"footprint_h": str(TINY_DIR / "sigma_footprint.h")},
    }


def tiny_sensor_demo(*, baseline: int, tolerance: int, reading: int) -> Dict[str, Any]:
    from cos.sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update_q16

    def sensor_sigma_q16(b: int, tol: int, r: int) -> int:
        if tol <= 0:
            return 65535
        diff = abs(r - b)
        sg = (diff * 65536) // tol
        return max(0, min(65535, sg))

    st = SigmaState()
    q = sensor_sigma_q16(baseline, tolerance, reading)
    k_raw = int(round(0.9 * 65536))
    sigma_update_q16(st, q, k_raw)
    v = sigma_gate(st)
    return {
        "baseline": baseline,
        "tolerance": tolerance,
        "reading": reading,
        "sigma_q16": q,
        "verdict": Verdict(v).name,
        "verdict_code": int(v),
        "note": "mirrors hw/tinyml/sigma_sensor.c mapping + sigma_gate_core",
    }


def tiny_flash_hint(target: str, port: str) -> Dict[str, Any]:
    return {
        "ok": False,
        "skipped": True,
        "target": target,
        "port": port,
        "hint": "Use Arduino IDE / PlatformIO / idf.py for flash; in-tree CLI does not invoke a programmer.",
    }


__all__ = [
    "REPO_ROOT",
    "TINY_DIR",
    "tiny_flash_hint",
    "tiny_footprint_json",
    "tiny_generate_info",
    "tiny_sensor_demo",
]
