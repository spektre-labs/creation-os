# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""One-command onboarding: probe the host, pick a local **Ollama** model tag, optionally
``ollama pull``, run :class:`~cos.boot.Boot`, then :func:`~cos.agi_demo.run_demo`.

This is **lab convenience** only: it does not start ``llama.cpp`` servers, does not prove
fitness for your workload, and is **NOT AGI**. The portable floor remains ``sigma_gate.h`` /
:class:`~cos.sigma_gate.SigmaGate` (see ``docs/CLAIM_DISCIPLINE.md``).

Environment:

- ``COS_SETUP_SKIP_PULL=1`` — same as ``cos setup --skip-pull`` (CI / air-gap).
"""
from __future__ import annotations

import json
import os
import shutil
import subprocess
from typing import Any, Dict, Optional

from cos.edge import EdgeProfile

__all__ = ["detect_hardware", "has_ollama", "pick_model", "setup"]


def detect_hardware() -> Dict[str, Any]:
    """Best-effort GPU, VRAM, RAM, OS, and CPU architecture (heuristics only)."""
    prof = EdgeProfile.detect()
    ram_gb = round(float(prof["ram_mb"]) / 1024.0, 1)
    hw: Dict[str, Any] = {
        "gpu": None,
        "vram_gb": 0.0,
        "ram_gb": ram_gb,
        "os": str(prof["platform"]),
        "arch": str(prof["cpu"]),
        "tier": int(prof["tier"]),
    }

    try:
        r = subprocess.run(
            [
                "nvidia-smi",
                "--query-gpu=name,memory.total",
                "--format=csv,noheader,nounits",
            ],
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )
        if r.returncode == 0 and r.stdout.strip():
            line = r.stdout.strip().splitlines()[0]
            parts = [p.strip() for p in line.split(",")]
            if len(parts) >= 2:
                hw["gpu"] = parts[0]
                # `memory.total` is reported in MiB for this query format.
                hw["vram_gb"] = round(int(float(parts[1])) / 1024.0, 1)
    except Exception:
        pass

    if not hw["gpu"] and hw["os"] == "Darwin" and "arm" in hw["arch"].lower():
        hw["gpu"] = "Apple Silicon (Metal)"
        hw["vram_gb"] = float(hw["ram_gb"])

    return hw


def pick_model(hw: Dict[str, Any]) -> Dict[str, Any]:
    """Map coarse memory heuristics to an Ollama model tag (may need a catalog tweak)."""
    vram = float(hw.get("vram_gb") or 0.0)
    ram = float(hw.get("ram_gb") or 0.0)

    if vram >= 16.0 or ram >= 32.0:
        return {
            "model": "qwen3:8b",
            "quant": "Q4_K_M",
            "size_gb": 4.5,
            "speed": "fast",
        }
    if vram >= 8.0 or ram >= 16.0:
        return {
            "model": "qwen3:4b",
            "quant": "Q4_K_M",
            "size_gb": 2.5,
            "speed": "fast",
        }
    if vram >= 4.0 or ram >= 8.0:
        return {
            "model": "gemma3:1b",
            "quant": "Q4_K_M",
            "size_gb": 0.8,
            "speed": "moderate",
        }
    return {
        "model": "smollm2:135m",
        "quant": "Q4_K_M",
        "size_gb": 0.1,
        "speed": "slow but works",
    }


def has_ollama() -> bool:
    """Return True if the ``ollama`` binary is on ``PATH``."""
    return shutil.which("ollama") is not None


def setup(
    *,
    skip_ollama_pull: bool = False,
    skip_demo: bool = False,
    json_out: bool = False,
) -> int:
    """Run detect -> model -> optional Ollama pull -> :class:`~cos.boot.Boot` -> demo."""
    env_skip = os.environ.get("COS_SETUP_SKIP_PULL", "").strip().lower()
    if env_skip in ("1", "true", "yes"):
        skip_ollama_pull = True

    hw = detect_hardware()
    model = pick_model(hw)

    pull_status = "skipped"
    pull_returncode: Optional[int] = None

    if json_out:
        out: Dict[str, Any] = {
            "hardware": hw,
            "model_plan": model,
            "ollama_on_path": has_ollama(),
        }
    else:
        print("=" * 60)
        print("CREATION OS — AUTO SETUP")
        print("=" * 60)

        print("\n[1/5] Detecting hardware...")
        print(f"  OS: {hw['os']} ({hw['arch']})")
        print(f"  RAM: {hw['ram_gb']} GiB")
        print(f"  GPU: {hw['gpu'] or 'CPU only'}")
        if float(hw.get("vram_gb") or 0.0) > 0.0:
            print(f"  VRAM (or unified): {hw['vram_gb']} GiB")

        print("\n[2/5] Selecting model...")
        print(f"  Model: {model['model']}")
        print(f"  Size (est.): {model['size_gb']} GiB")
        print(f"  Speed: {model['speed']}")

        print("\n[3/5] Downloading model (Ollama)...")

    if has_ollama():
        if not skip_ollama_pull:
            if not json_out:
                print(f"  ollama pull {model['model']}")
            try:
                r = subprocess.run(
                    ["ollama", "pull", model["model"]],
                    capture_output=json_out,
                    timeout=3600,
                    check=False,
                )
                pull_returncode = int(r.returncode)
                if r.returncode == 0:
                    pull_status = "ok"
                    if not json_out:
                        print("  [ok] Download finished")
                else:
                    pull_status = "failed"
                    if not json_out:
                        print("  [fail] Download failed — continue with sigma-gate / partial runtime")
            except (OSError, subprocess.SubprocessError) as exc:
                pull_status = f"error:{exc}"
                if not json_out:
                    print(f"  [fail] {exc} — continue with sigma-gate only where possible")
        else:
            pull_status = "skipped_by_flag"
            if not json_out:
                print("  Skipped (--skip-pull or COS_SETUP_SKIP_PULL).")
    else:
        pull_status = "ollama_missing"
        if not json_out:
            print("  Ollama not found. Install: https://ollama.com/download")
            print("  Continuing without `ollama pull` (sigma-gate still works).")

    boot_result: Dict[str, Any] = {}
    if not json_out:
        print("\n[4/5] Booting Creation OS...")
    try:
        from cos.boot import Boot

        boot_result = Boot().run()
        if json_out:
            out["boot"] = boot_result
        else:
            print(f"  {boot_result.get('message', '')}")
    except Exception as ex:  # noqa: BLE001
        boot_result = {"error": str(ex)}
        if json_out:
            out["boot"] = boot_result
        else:
            print(f"  Boot: {ex}")
            print("  Sigma-gate remains available: cos score --prompt ... --response ...")

    demo_ran = False
    demo_error: Optional[str] = None
    if not skip_demo:
        if not json_out:
            print("\n[5/5] Running lab demo (agi-demo)...")
        try:
            from cos.agi_demo import run_demo

            run_demo()
            demo_ran = True
        except Exception as ex:  # noqa: BLE001
            demo_error = str(ex)
            if not json_out:
                print(f"  Demo error: {ex}")
            from cos.sigma_gate import SigmaGate

            gate = SigmaGate()
            sigma, verdict = gate.score("What is 2+2?", "4")
            if not json_out:
                print(f"  sigma={sigma:.3f} {verdict} — sigma-gate smoke OK")
    else:
        if not json_out:
            print("\n[5/5] Skipping agi-demo (--skip-demo).")

    if json_out:
        out["ollama_pull"] = {
            "status": pull_status,
            "returncode": pull_returncode,
        }
        out["demo"] = {
            "ran": demo_ran,
            "skipped": skip_demo,
            "error": demo_error,
        }
        print(json.dumps(out, ensure_ascii=False, default=str))
        return 0

    print("\n" + "=" * 60)
    print("SETUP COMPLETE")
    print("=" * 60)
    footer = f"""
Next steps:
  cos score --prompt "test" --response "test"
  cos chat
  cos agi-demo
  cos eval-all

Hardware: {hw["gpu"] or "CPU"} / {hw["ram_gb"]} GiB RAM
Model (planned): {model["model"]}
Sigma-gate: ACTIVE

NOT AGI ACHIEVED. σ. 1=1.
""".strip()
    print(footer)
    return 0
