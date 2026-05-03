# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v159 σ-silicon lab: paths to Xsigma RTL + optional Yosys, semantic smoke vs sigma_gate_core.

Does not implement full core integration; see hw/riscv/sigma_isa.md.
"""
from __future__ import annotations

import re
import subprocess
from pathlib import Path
from typing import Any, Dict, List, Tuple

from cos.sigma_gate_core import K_CRIT, SigmaState, Verdict, sigma_gate, sigma_update_q16

REPO_ROOT = Path(__file__).resolve().parents[2]
RTL_PATH = REPO_ROOT / "hw" / "riscv" / "sigma_core.v"
ISA_PATH = REPO_ROOT / "hw" / "riscv" / "sigma_isa.md"
BENCH_MD = REPO_ROOT / "hw" / "riscv" / "benchmark.md"
YOSYS_SCRIPT = REPO_ROOT / "hw" / "riscv" / "yosys_sigma_cfu.ys"


def rtl_generate_info(*, target: str) -> Dict[str, Any]:
    """Report canonical RTL paths (no code generation — golden file lives in-tree)."""
    tgt = (target or "generic").strip().lower()
    return {
        "generated": False,
        "target": tgt,
        "note": "Golden RTL is committed at hw/riscv/sigma_core.v (module sigma_rv_cfu).",
        "paths": {
            "rtl": str(RTL_PATH),
            "isa": str(ISA_PATH),
            "benchmarks_md": str(BENCH_MD),
        },
    }


def parse_sim_test(spec: str) -> Tuple[str, List[str]]:
    spec = spec.strip()
    if not spec:
        return "", []
    parts = spec.split()
    if not parts:
        return "", []
    return parts[0].lower(), parts[1:]


def simulate_semantic(op: str, args: List[str]) -> Dict[str, Any]:
    """Behavioral reference: sigma_gate_core Q16 state (not cycle-accurate RTL cosim)."""
    if op != "sigma_update":
        return {"ok": False, "reason": f"unknown op {op!r} (try: sigma_update <new_sigma_hex> <k_raw_hex>)"}
    if len(args) < 2:
        return {"ok": False, "reason": "sigma_update needs two hex arguments"}
    new_s = int(args[0], 0)
    k_raw = int(args[1], 0)
    st = SigmaState()
    sigma_update_q16(st, new_s & 0xFFFF_FFFF, k_raw & 0xFFFF_FFFF)
    v = sigma_gate(st)
    vname = Verdict(v).name if v in (0, 1, 2) else "?"
    return {
        "ok": True,
        "op": op,
        "new_sigma_q16": new_s,
        "k_raw_q16": k_raw,
        "sigma_q16": st.sigma,
        "d_sigma_q16": st.d_sigma,
        "k_eff_q16": st.k_eff,
        "K_CRIT": K_CRIT,
        "verdict": vname,
        "verdict_code": int(v),
        "note": "Python sigma_gate_core reference — compare RTL in Verilator cosim separately",
    }


def run_yosys_synth(*, target: str) -> Dict[str, Any]:
    import shutil

    ys = shutil.which("yosys")
    if not ys:
        return {"ok": False, "skipped": True, "reason": "yosys not on PATH"}
    if not YOSYS_SCRIPT.is_file():
        return {"ok": False, "reason": f"missing {YOSYS_SCRIPT}"}
    r = subprocess.run(
        [ys, "-s", str(YOSYS_SCRIPT.name)],
        cwd=str(YOSYS_SCRIPT.parent),
        capture_output=True,
        text=True,
        check=False,
    )
    out = (r.stdout or "") + (r.stderr or "")
    cells = None
    for line in out.splitlines():
        if "Number of cells" in line or "cells:" in line.lower():
            m = re.search(r"([0-9]+)", line)
            if m:
                cells = int(m.group(1))
                break
    return {
        "ok": r.returncode == 0,
        "target": target,
        "exit_code": r.returncode,
        "cells_parsed": cells,
        "log_tail": out[-1800:] if len(out) > 1800 else out,
        "note": "Yosys generic synthesis — not ECP5/nextpnr place-and-route",
    }


def benchmark_targets_json() -> Dict[str, Any]:
    """Design targets only (see hw/riscv/benchmark.md evidence class)."""
    return {
        "evidence_class": "lab / roadmap targets (not measured FPGA here)",
        "software_cycles_order_of_magnitude": {
            "sigma_update": "8–15",
            "sigma_verdict": "4–10",
            "ternary_one_weight": "5–8",
        },
        "hardware_target_cycles": {
            "SIGMA_UPDATE": 1,
            "SIGMA_GATE": 1,
            "TERN_MATVEC_one_weight": 1,
        },
        "speedup_note": "~6–8× on update+verdict micro-sequence vs scalar C is a budget, not a guarantee",
        "paths": {"benchmark_md": str(BENCH_MD)},
    }


def verilator_lint_available() -> bool:
    import shutil

    return shutil.which("verilator") is not None


__all__ = [
    "BENCH_MD",
    "ISA_PATH",
    "RTL_PATH",
    "benchmark_targets_json",
    "rtl_generate_info",
    "run_yosys_synth",
    "simulate_semantic",
    "verilator_lint_available",
]
