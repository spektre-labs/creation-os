#!/usr/bin/env python3
"""Generate an operator-facing EU AI Act *mapping* report from local audit JSONL.

This is documentation support only — not legal advice or a conformity certificate.
Metrics that are not in the audit trail are pulled from repository artifacts when
present (see docs/CLAIM_DISCIPLINE.md).
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


def _home() -> Path:
    return Path(os.environ.get("HOME", "/tmp"))


def _audit_glob() -> List[Path]:
    d = _home() / ".cos" / "audit"
    if not d.is_dir():
        return []
    return sorted(d.glob("*.jsonl"))


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _parse_results_md(p: Path) -> Dict[str, Optional[float]]:
    out: Dict[str, Optional[float]] = {
        "auroc": None,
        "aurc": None,
        "augrc": None,
        "brier": None,
        "ece": None,
    }
    if not p.is_file():
        return out
    text = p.read_text(encoding="utf-8", errors="replace")
    for key, pat in (
        ("auroc", r"\*\*AUROC:\*\*\s*([0-9.]+)"),
        ("aurc", r"\*\*AURC:\*\*\s*([0-9.]+)"),
        ("augrc", r"\*\*AUGRC:\*\*\s*([0-9.]+)"),
        ("brier", r"\*\*Brier:\*\*\s*([0-9.]+)"),
    ):
        m = re.search(pat, text)
        if m:
            out[key] = float(m.group(1))
    # ECE may appear as **ECE** or in prose — optional line
    m = re.search(r"\*\*ECE\*\*:\s*([0-9.]+)", text)
    if m:
        out["ece"] = float(m.group(1))
    return out


def _wrong_confident_csv(
    csv_path: Path, sigma_threshold: float
) -> Tuple[Optional[int], Optional[int], str]:
    """Count incorrect rows; 'confident wrong' = incorrect with sigma < threshold."""
    if not csv_path.is_file():
        return None, None, "graded CSV not found at this path"
    wrong = 0
    wrong_conf = 0
    with csv_path.open(newline="", encoding="utf-8") as f:
        r = csv.DictReader(f)
        for row in r:
            try:
                ok = int(row.get("is_correct", -1))
                sig = float(row.get("sigma", "nan"))
            except (TypeError, ValueError):
                continue
            if ok == 0:
                wrong += 1
                if sig == sig and sig < sigma_threshold:
                    wrong_conf += 1
    note = (
        f"From {csv_path.name}: incorrect={wrong}; "
        f"subset with sigma<{sigma_threshold:g} (operator-defined 'confident')={wrong_conf}. "
        "Not a regulatory metric unless your DPIA defines it."
    )
    return wrong, wrong_conf, note


def _sigma_gap_from_docs() -> Optional[str]:
    p = _repo_root() / "benchmarks" / "DEFINITIVE_RESULTS.md"
    if not p.is_file():
        return None
    m = re.search(r"Gap:\s*([0-9.]+)", p.read_text(encoding="utf-8", errors="replace"))
    return m.group(1) if m else None


def _load_jsonl(paths: List[Path]) -> List[Dict[str, Any]]:
    rows: List[Dict[str, Any]] = []
    for path in paths:
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        for line in lines:
            line = line.strip()
            if not line:
                continue
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError:
                continue
    return rows


def _verify_chain(lines: List[str]) -> Tuple[bool, str]:
    if not lines:
        return True, "no JSONL lines to verify"
    prev_raw: Optional[bytes] = None
    for i, raw in enumerate(lines):
        raw = raw.rstrip("\r\n")
        if not raw.strip():
            continue
        b = raw.encode("utf-8") + b"\n"
        try:
            obj = json.loads(raw)
        except json.JSONDecodeError:
            return False, f"invalid JSON on logical line {i + 1}"
        cp = obj.get("chain_prev") or ""
        if prev_raw is not None:
            hx = hashlib.sha256(prev_raw).hexdigest()
            expect = f"sha256:{hx}"
            if cp != expect:
                return False, f"chain break after line {i + 1} (expected {expect[:24]}…)"
        prev_raw = b
    return True, "OK"


def main() -> int:
    ap = argparse.ArgumentParser(description="EU AI Act mapping report from audit JSONL")
    ap.add_argument(
        "--out",
        default="reports/eu_ai_act_conformity.md",
        help="Output markdown path (default: reports/eu_ai_act_conformity.md)",
    )
    ap.add_argument(
        "--metrics-md",
        default="",
        help="Override path to graded RESULTS.md (default: benchmarks/graded/RESULTS.md under repo root)",
    )
    ap.add_argument(
        "--graded-csv",
        default="",
        help="Override graded_results.csv for wrong/confident heuristic",
    )
    args = ap.parse_args()
    root = _repo_root()
    out_path = Path(args.out)
    if not out_path.is_absolute():
        out_path = root / out_path
    out_path.parent.mkdir(parents=True, exist_ok=True)

    audit_files = _audit_glob()
    raw_lines: List[str] = []
    for p in audit_files:
        raw_lines.extend(p.read_text(encoding="utf-8", errors="replace").splitlines())

    chain_ok, chain_msg = _verify_chain([ln for ln in raw_lines if ln.strip()])
    rows = _load_jsonl(audit_files)

    n = len(rows)
    abst = sum(1 for r in rows if r.get("action") == "ABSTAIN")
    acc = sum(1 for r in rows if r.get("action") == "ACCEPT")
    ret = sum(1 for r in rows if r.get("action") == "RETHINK")
    blocked_pct = (100.0 * abst / n) if n else 0.0

    ts_first = ts_last = "—"
    if rows:
        ts_first = str(rows[0].get("timestamp", "—"))
        ts_last = str(rows[-1].get("timestamp", "—"))

    codex_h = "—"
    for r in reversed(rows):
        if r.get("codex_hash"):
            codex_h = str(r["codex_hash"])
            break

    tau_acc = "—"
    for r in reversed(rows):
        if "tau_accept" in r:
            tau_acc = str(r["tau_accept"])
            break

    metrics_path = Path(args.metrics_md) if args.metrics_md else root / "benchmarks" / "graded" / "RESULTS.md"
    mets = _parse_results_md(metrics_path)

    csv_path = Path(args.graded_csv) if args.graded_csv else root / "benchmarks" / "graded" / "graded_results.csv"
    wrong, wrong_conf, wc_note = _wrong_confident_csv(csv_path, 0.35)
    gap = _sigma_gap_from_docs()

    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    ver = os.environ.get("COS_REPORT_VERSION", "see `cos --version`")

    lines_out = [
        "# EU AI Act — operator mapping report (Creation OS)",
        "",
        "**Disclaimer:** This file maps local telemetry to Articles 9, 13, 14, and 15 "
        "headings for internal governance workflows. It is **not** a legal conformity "
        "assessment and does not satisfy notified-body or market-surveillance obligations "
        "by itself.",
        "",
        f"| Field | Value |",
        f"|--------|--------|",
        f"| System | Creation OS ({ver}) |",
        f"| Assessment window (audit) | {ts_first} → {ts_last} |",
        f"| Generated (UTC) | {now} |",
        f"| Audit files read | {len(audit_files)} JSONL path(s) under `~/.cos/audit/` |",
        "",
        "## Article 9: Risk management (telemetry)",
        "",
        f"- Total gated queries logged: **{n}**",
        f"- ABSTAIN (blocked) count: **{abst}** ({blocked_pct:.1f}% of logged queries)",
        f"- ACCEPT: **{acc}**, RETHINK: **{ret}**",
        "",
    ]

    if mets.get("auroc") is not None:
        try:
            mrel = metrics_path.relative_to(root)
        except ValueError:
            mrel = metrics_path
        lines_out.append(
            f"- **AUROC** (lab, graded export — evidence class: see `{mrel}`): **{mets['auroc']:.4f}**"
        )
    else:
        lines_out.append(
            "- **AUROC:** not embedded (run graded export or set `--metrics-md` to RESULTS.md)"
        )
    lines_out.append(f"- **Wrong / confident (heuristic):** {wc_note}")
    lines_out.append(
        "- **Risk band:** operator judgement from ABSTAIN rate + drift flags in `cos monitor --dashboard` / ledger — not auto-classified as Annex III 'high-risk' here."
    )
    lines_out.extend(
        [
            "",
            "## Article 13: Transparency",
            "",
            "- Each append-only audit row includes `audit_id`, `timestamp`, `model`, σ, thresholds, hashes, and optional `chain_prev`.",
            f"- Latest codex hash observed in audit: `{codex_h}`",
            "",
            "## Article 14: Human oversight",
            "",
            f"- Latest logged τ_accept: **{tau_acc}** (from audit row when present; else conformal bundle on the serving host).",
            f"- ABSTAIN rate over window: **{blocked_pct:.1f}%**",
            "- Escalation / manual review paths are documented in operator runbooks (not generated here).",
            "",
            "## Article 15: Accuracy, robustness, cybersecurity (bounded numbers)",
            "",
        ]
    )
    if mets.get("auroc") is not None:
        lines_out.append(f"- **AUROC (graded run):** {mets['auroc']:.4f}")
    if mets.get("ece") is not None:
        lines_out.append(f"- **ECE (when present in RESULTS.md):** {mets['ece']:.4f}")
    if mets.get("brier") is not None:
        lines_out.append(f"- **Brier (graded run):** {mets['brier']:.4f}")
    if gap:
        lines_out.append(
            f"- **σ-separation gap (spot-check artefact, see DEFINITIVE_RESULTS.md):** {gap}"
        )
    lines_out.append(
        "- **Metacognitive profile / harness claims:** bind any headline to the evidence class in `docs/CLAIM_DISCIPLINE.md` — do not merge lab AUROC with frontier harness rows."
    )
    lines_out.extend(
        [
            "",
            "## Audit trail integrity",
            "",
            f"- Append-only JSONL entries scanned: **{n}**",
            f"- Chain integrity check: **{'PASS' if chain_ok else 'FAIL'}** — {chain_msg}",
            "",
            "---",
            "*Spektre Labs · Creation OS · compliance mapping script*",
        ]
    )

    out_path.write_text("\n".join(lines_out) + "\n", encoding="utf-8")
    print(str(out_path))
    return 0


if __name__ == "__main__":
    sys.exit(main())
