#!/usr/bin/env python3
"""
Merge **local** σ-gate evaluation JSON summaries into one printable table.

**Literature** rows (HaMI / baselines) are labeled explicitly — they are **not** values produced
by this repository unless a matching harness JSON exists. See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any, Dict, Optional, Tuple


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _load(path: Path) -> Optional[Dict[str, Any]]:
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return None


def _fmt(x: Any) -> str:
    if x is None:
        return "—"
    if isinstance(x, float) and (math.isnan(x) or math.isinf(x)):
        return "nan"
    if isinstance(x, float):
        return f"{x:.4f}"
    if isinstance(x, dict) and "error" in x:
        return "err"
    return str(x)


def _holdout_auroc(data: Optional[Dict[str, Any]]) -> Optional[float]:
    if not data:
        return None
    v = data.get("auroc_wrong_vs_sigma")
    return float(v) if isinstance(v, (int, float)) and not (isinstance(v, float) and math.isnan(v)) else None


def _cross_trivia_halu(data: Optional[Dict[str, Any]]) -> Tuple[Optional[float], Optional[float]]:
    if not data:
        return None, None
    t = data.get("triviaqa_auroc_wrong_vs_sigma")
    h = data.get("halueval_auroc_wrong_vs_sigma")
    def f(v: Any) -> Optional[float]:
        if isinstance(v, (int, float)) and not (isinstance(v, float) and math.isnan(v)):
            return float(v)
        return None
    return f(t), f(h)


def _multi_auroc_block(data: Optional[Dict[str, Any]]) -> Dict[str, Optional[float]]:
    """Parse ``aurocs_wrong_vs_score`` blocks from hide/ultimate-style summaries."""
    out: Dict[str, Optional[float]] = {}
    if not data:
        return out
    inner = data.get("aurocs_wrong_vs_score")
    if not isinstance(inner, dict):
        return out
    for name, block in inner.items():
        if isinstance(block, (int, float)):
            out[str(name)] = float(block) if not (isinstance(block, float) and math.isnan(block)) else None
        elif isinstance(block, dict) and "error" in block and len(block) <= 2:
            out[str(name)] = None
        elif isinstance(block, dict):
            v = block.get("auroc") or block.get("AUROC")
            out[str(name)] = float(v) if isinstance(v, (int, float)) else None
        else:
            out[str(name)] = None
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--base",
        type=Path,
        default=None,
        help="Repository root (default: parent of benchmarks/)",
    )
    args = ap.parse_args()
    root = Path(args.base).resolve() if args.base else _repo_root()
    ev = root / "benchmarks" / "sigma_gate_eval"

    hold = _load(ev / "results_holdout" / "holdout_summary.json")
    cross = _load(ev / "results_cross_domain" / "cross_domain_summary.json")
    hide = _load(ev / "results_hide" / "hide_summary.json")
    ult = _load(ev / "results_ultimate" / "ultimate_summary.json")
    hami = _load(ev / "results_hami" / "hami_summary.json")
    multi = _load(ev / "results_multi_signal" / "multi_signal_summary.json")

    t_hold = _holdout_auroc(hold)
    t_trivia, t_halu = _cross_trivia_halu(cross)

    hide_m = _multi_auroc_block(hide) if hide else {}
    ult_m = _multi_auroc_block(ult) if ult else {}
    hami_m = _multi_auroc_block(hami) if hami else {}

    def col_truth(alt: Dict[str, Optional[float]]) -> Optional[float]:
        for k in ("truthfulqa_holdout", "holdout", "holdout_full"):
            if k in alt and alt[k] is not None:
                return alt[k]
        return None

    def col_trivia(primary: Optional[float], alt: Dict[str, Optional[float]]) -> Optional[float]:
        if primary is not None:
            return primary
        v = alt.get("triviaqa")
        return v

    def col_halu(primary: Optional[float], alt: Dict[str, Optional[float]]) -> Optional[float]:
        if primary is not None:
            return primary
        for k in ("halueval_generative", "halueval", "halu"):
            if k in alt and alt[k] is not None:
                return alt[k]
        return None

    rows = [
        ("LSD (holdout JSON)", t_hold, None, None),
        ("LSD (cross_domain JSON)", None, t_trivia, t_halu),
        ("HIDE (results_hide)", col_truth(hide_m), col_trivia(None, hide_m), col_halu(None, hide_m)),
        ("Ultimate (results_ultimate)", col_truth(ult_m), col_trivia(None, ult_m), col_halu(None, ult_m)),
        ("HaMI (results_hami)", col_truth(hami_m), col_trivia(None, hami_m), col_halu(None, hami_m)),
    ]

    print()
    print("=" * 76)
    print("  σ-gate harness: merged AUROC (wrong=1 vs score), local JSON only")
    print("=" * 76)
    print(f"  {'Source':<34} {'TruthfulQA H':>12} {'TriviaQA':>12} {'HaluEval':>12}")
    print("  " + "-" * 72)
    for label, a, b, c in rows:
        print(f"  {label:<34} {_fmt(a):>12} {_fmt(b):>12} {_fmt(c):>12}")
    if multi:
        print("  " + "-" * 72)
        print(
            f"  {'multi_signal (holdout smoke)':<34} "
            f"{_fmt(multi.get('auroc_wrong_vs_sigma_lsd')):>12} "
            f"{_fmt(multi.get('auroc_wrong_vs_sigma_spectral')):>12} "
            f"{_fmt(multi.get('auroc_wrong_vs_sigma_unified')):>12}"
        )
    print("=" * 76)

    print()
    print("Literature anchors (paper-reported figures; not produced by this harness)")
    print("-" * 76)
    lit = [
        ("HaMI — reported TriviaQA-style AUROC (see arXiv:2504.07863 / venue version)", "0.886", "setup differs from this harness"),
        ("Semantic entropy / SelfCheckGPT / etc.", "—", "see primary sources cited in HaMI/HIDE papers"),
    ]
    for a, b, c in lit:
        print(f"  {a}")
        print(f"    example figure from literature: {b}  ({c})")
    print()


if __name__ == "__main__":
    main()
