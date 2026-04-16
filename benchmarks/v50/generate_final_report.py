#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Aggregate v50 benchmark directory into benchmarks/v50/FINAL_RESULTS.md."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def _read_json(p: Path) -> dict | None:
    try:
        return json.loads(p.read_text(encoding="utf-8"))
    except Exception:
        return None


def _stub_cell(obj: dict | None) -> str:
    if not obj:
        return "___"
    if obj.get("status") == "STUB":
        return "STUB"
    return "___"


def _certify_status(log_text: str) -> str:
    if "certify: OK" in log_text:
        return "OK (see log for SKIPs)"
    return "SEE_LOG"


def _grep_hits(log_text: str, needle: str) -> bool:
    return needle in log_text


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--results-dir", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()

    d: Path = args.results_dir
    out: Path = args.output

    tqa = _read_json(d / "truthfulqa_mc1.json")
    mmlu = _read_json(d / "mmlu_mini.json")
    arc = _read_json(d / "arc_challenge.json")
    hella = _read_json(d / "hellaswag.json")
    wino = _read_json(d / "winogrande.json")
    cal = _read_json(d / "calibration.json")
    lat = _read_json(d / "latency.json")
    mem = _read_json(d / "memory.json")

    certify_log = (d / "certify.log").read_text(encoding="utf-8", errors="replace") if (d / "certify.log").is_file() else ""
    mcdc_log = (d / "mcdc.log").read_text(encoding="utf-8", errors="replace") if (d / "mcdc.log").is_file() else ""
    audit_log = (d / "binary_audit.log").read_text(encoding="utf-8", errors="replace") if (d / "binary_audit.log").is_file() else ""

    lines: list[str] = []
    lines.append("# Creation OS v50 — Final Results")
    lines.append("")
    lines.append("## The question this report answers")
    lines.append("")
    lines.append(
        "Can a small ternary-oriented stack with σ-gating be **more useful** than a frontier model **without** selective reliability metrics? "
        "This file is the **single rollup surface**; until the engine harness is wired, Category 1–3 cells remain **STUB** (explicitly, not silently)."
    )
    lines.append("")
    lines.append("## Table 1: Accuracy comparison (slots)")
    lines.append("")
    lines.append("| Benchmark | BitNet 2B (no σ) | BitNet 2B + σ | Frontier ref |")
    lines.append("|-----------|------------------|---------------|--------------|")
    lines.append(
        f"| TruthfulQA MC1 | {_stub_cell(tqa)} | {_stub_cell(tqa)} (answered) | literature / leaderboard (external) |"
    )
    lines.append(f"| MMLU (mini) | {_stub_cell(mmlu)} | {_stub_cell(mmlu)} (answered) | literature / leaderboard (external) |")
    lines.append(f"| ARC-Challenge | {_stub_cell(arc)} | {_stub_cell(arc)} (answered) | literature / leaderboard (external) |")
    lines.append(f"| HellaSwag | {_stub_cell(hella)} | {_stub_cell(hella)} (answered) | literature / leaderboard (external) |")
    lines.append(f"| Winogrande | {_stub_cell(wino)} | {_stub_cell(wino)} (answered) | literature / leaderboard (external) |")
    lines.append("")
    lines.append("## Table 2: σ-specific metrics (targets; not measured until harness exists)")
    lines.append("")
    lines.append("| Metric | Value | What it means |")
    lines.append("|--------|-------|---------------|")
    lines.append(f"| accuracy_total | {_stub_cell(cal)} | correct on ALL questions |")
    lines.append(f"| accuracy_answered | {_stub_cell(cal)} | correct on questions the model chose to answer |")
    lines.append(f"| abstention_rate | {_stub_cell(cal)} | refused / abstained mass |")
    lines.append(f"| calibration_gap | {_stub_cell(cal)} | |self-report − actual| (lower is better) |")
    lines.append(f"| hallucination_rate | {_stub_cell(cal)} | wrong answers among answered |")
    lines.append(f"| σ_overhead_ms | {_stub_cell(lat)} | added latency for σ telemetry |")
    lines.append(f"| σ_overhead_pct | {_stub_cell(lat)} | % latency increase |")
    lines.append("")
    lines.append("## Table 3: Performance (slots)")
    lines.append("")
    lines.append("| Metric | Value | Notes |")
    lines.append("|--------|-------|------|")
    lines.append(f"| Memory | {_stub_cell(mem)} | compare only with pinned harness + allocator |")
    lines.append(f"| Latency/token | {_stub_cell(lat)} | compare only with pinned harness |")
    lines.append("| Energy/token | ___ | needs instrumented power trace (external) |")
    lines.append("")
    lines.append("## Table 4: Verification status (from this run’s logs)")
    lines.append("")
    lines.append("| Layer | Method | Status |")
    lines.append("|-------|--------|--------|")
    lines.append(f"| σ-kernel C | Frama-C/WP | `{_certify_status(certify_log)}` |")
    lines.append(f"| σ-pipeline SV | SymbiYosys | `{'SKIP' if _grep_hits(certify_log, 'formal-sby-v47: SKIP') else 'SEE certify.log'}` |")
    lines.append(f"| MC/DC coverage | gcov | `{'OK' if 'certify-coverage: OK' in mcdc_log else 'SEE mcdc.log'}` |")
    lines.append(f"| Binary audit | audit script | `{'OK' if 'AUDIT COMPLETE' in audit_log else 'SEE binary_audit.log'}` |")
    lines.append("| Red team (Garak) | modules | `SKIP` unless installed + model endpoint |")
    lines.append("| Red team (σ-specific) | v48 catalog | `STUB` until aggregated JSON exists |")
    lines.append("| Traceability | manifest | run `python3 scripts/v49/verify_traceability.py` |")
    lines.append("")
    lines.append("## Table 5: Certification readiness (artifact posture)")
    lines.append("")
    lines.append("| Standard | Requirement | Status |")
    lines.append("|----------|-------------|--------|")
    lines.append("| DO-178C DAL-A | Formal methods (DO-333) | `PARTIAL` (Frama-C targets; authority not claimed) |")
    lines.append("| DO-178C DAL-A | MC/DC structural coverage | `PARTIAL` (driver exists; goals not claimed 100%) |")
    lines.append("| DO-178C DAL-A | Bidirectional traceability | `MET` for manifest-driven checks |")
    lines.append("| DO-178C DAL-A | Independent verification | `NOT MET` in-repo (no regulator) |")
    lines.append("| NIST AI RMF | Risk assessment documented | `PARTIAL` (see v48/v49 docs) |")
    lines.append("| OWASP LLM Top 10 | All risks addressed | `NOT CLAIMED` |")
    lines.append("| EU AI Act Art.15 | Accuracy/robustness/cyber | `NOT CLAIMED` without legal review |")
    lines.append("| ISO/IEC 42001 | AI MS | `PARTIAL` |")
    lines.append("")
    lines.append("## The claim (honest tiering)")
    lines.append("")
    lines.append(
        "Nothing in Table 1–3 is **M-tier** until archived JSON + REPRO bundle exist. "
        "Verification rows may be **M-tier** for “commands ran” but not for “all proofs discharged” unless logs show it."
    )
    lines.append("")
    lines.append("## How to reproduce (today)")
    lines.append("")
    lines.append("```bash")
    lines.append("git clone https://github.com/spektre-labs/creation-os")
    lines.append("cd creation-os")
    lines.append("make certify        # assurance stack (SKIPs OK)")
    lines.append("make red-team       # v48 harness (SKIPs OK)")
    lines.append("make v50-benchmark  # writes this report + stub JSON + logs")
    lines.append("```")
    lines.append("")

    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
