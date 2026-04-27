#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Aggregate σ ablation jsonl → summary JSON + markdown table + decision text."""

from __future__ import annotations

import argparse
import json
import math
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, DefaultDict, Dict, List

HERE = Path(__file__).resolve().parent
RESULTS_DIR = HERE / "results"


def ece_bins(conf: List[float], correct: List[int], n_bins: int = 10) -> float:
    if not conf:
        return float("nan")
    eps = 1e-6
    bins: List[List[Tuple[float, int]]] = [[] for _ in range(n_bins)]
    for cf, y in zip(conf, correct):
        cf = min(1.0 - eps, max(eps, cf))
        b = min(n_bins - 1, int(cf * n_bins))
        bins[b].append((cf, y))
    ece = 0.0
    tot = len(conf)
    for b in bins:
        if not b:
            continue
        acc = sum(y for _, y in b) / len(b)
        cm = sum(c for c, _ in b) / len(b)
        ece += (len(b) / tot) * abs(acc - cm)
    return float(ece)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--detail", type=Path, default=RESULTS_DIR / "sigma_ablation_detail.jsonl")
    ap.add_argument("--summary", type=Path, default=RESULTS_DIR / "sigma_ablation_summary.json")
    ap.add_argument("--table", type=Path, default=RESULTS_DIR / "sigma_ablation_table.md")
    ap.add_argument("--tau-wrong-conf", type=float, default=0.3)
    args = ap.parse_args()

    if not args.detail.is_file():
        print(f"error: missing {args.detail} (run run_sigma_ablation.py first)", file=sys.stderr)
        return 2

    try:
        from sklearn.metrics import average_precision_score, roc_auc_score
    except ImportError:
        print("error: pip install scikit-learn", file=sys.stderr)
        return 2

    rows: List[Dict[str, Any]] = []
    with args.detail.open(encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            rows.append(json.loads(line))

    groups: DefaultDict[Tuple[str, int, str], List[Dict[str, Any]]] = defaultdict(list)
    for r in rows:
        key = (str(r["model"]), int(r["n_samples"]), str(r["signal"]))
        groups[key].append(r)

    per_group: List[Dict[str, Any]] = []
    for (model, n_samp, signal), recs in sorted(groups.items()):
        y_wrong = [1 - int(x["correct"]) for x in recs]
        s = [float(x["sigma"]) for x in recs]
        acc_flags = [bool(x["accepted"]) for x in recs]
        corr = [int(x["correct"]) for x in recs]
        if len(set(y_wrong)) < 2:
            auroc = float("nan")
            auprc = float("nan")
        else:
            auroc = float(roc_auc_score(y_wrong, s))
            auprc = float(average_precision_score(y_wrong, s))
        conf = [max(1e-6, min(1.0 - 1e-6, 1.0 - float(x["sigma"]))) for x in recs]
        ece = ece_bins(conf, corr, 10)
        n_acc = sum(1 for a in acc_flags if a)
        acc_accept = (
            sum(c for c, a in zip(corr, acc_flags) if a) / n_acc if n_acc else float("nan")
        )
        abst = 1.0 - (n_acc / max(len(recs), 1))
        cov = n_acc / max(len(recs), 1)
        wconf = sum(
            1
            for x in recs
            if x["accepted"]
            and not x["correct"]
            and float(x["sigma"]) < args.tau_wrong_conf
        )
        sc = [float(x["sigma"]) for x in recs if x["correct"]]
        sw = [float(x["sigma"]) for x in recs if not x["correct"]]
        m_c = sum(sc) / len(sc) if sc else float("nan")
        m_w = sum(sw) / len(sw) if sw else float("nan")
        gap = (m_w - m_c) if (sc and sw) else float("nan")
        per_group.append(
            {
                "model": model,
                "n_samples": n_samp,
                "signal": signal,
                "n_rows": len(recs),
                "auroc": auroc,
                "auprc": auprc,
                "ece": ece,
                "accuracy_accepted": acc_accept,
                "coverage": cov,
                "abstain_rate": abst,
                "wrong_confident_count": wconf,
                "mean_sigma_correct": m_c,
                "mean_sigma_wrong": m_w,
                "sigma_gap": gap,
            }
        )

    def best_for_model(mid: str) -> Dict[str, Any]:
        sub = [x for x in per_group if x["model"] == mid and not math.isnan(x["auroc"])]
        if not sub:
            return {}
        b = max(sub, key=lambda z: z["auroc"])
        return b

    models = sorted({x["model"] for x in per_group})
    best_by_model = {m: best_for_model(m) for m in models}
    overall = [x for x in per_group if not math.isnan(x["auroc"])]
    best_overall = max(overall, key=lambda z: z["auroc"]) if overall else {}

    def _auroc(d: Dict[str, Any]) -> float:
        v = d.get("auroc") if d else None
        if not isinstance(v, (int, float)) or (isinstance(v, float) and math.isnan(v)):
            return float("nan")
        return float(v)

    bit = _auroc(best_by_model.get("bitnet_2b_current", {}))
    gem = _auroc(best_by_model.get("gemma3_4b", {}))
    phi = _auroc(best_by_model.get("phi4", {}))

    conclusions: List[str] = []
    if not math.isnan(gem) and not math.isnan(bit) and gem - bit >= 0.10:
        conclusions.append(
            "Model-size signal: best gemma3_4b AUROC exceeds bitnet_2b_current by ≥0.10 "
            "(small / local model limitation plausible)."
        )
    if not math.isnan(phi) and not math.isnan(bit) and phi - bit >= 0.10:
        conclusions.append(
            "Model-size signal: phi4 best AUROC exceeds bitnet by ≥0.10."
        )

    by_n: Dict[int, float] = {}
    for ns in (3, 5, 8, 10):
        vals = [
            x["auroc"]
            for x in per_group
            if x["n_samples"] == ns and not math.isnan(float(x["auroc"]))
        ]
        by_n[ns] = max(vals) if vals else float("nan")
    n3 = by_n.get(3, float("nan"))
    n8 = by_n.get(8, float("nan"))
    n10 = by_n.get(10, float("nan"))
    if not math.isnan(n3) and not math.isnan(n8) and n8 - n3 >= 0.05:
        conclusions.append(
            "Sample-count signal: n=8 AUROC improves over n=3 by ≥0.05 on the best arm."
        )
    if not math.isnan(n3) and not math.isnan(n10) and n10 - n3 >= 0.05:
        conclusions.append(
            "Sample-count signal: n=10 AUROC improves over n=3 by ≥0.05 on the best arm."
        )

    best_sem = max(
        (x["auroc"] for x in per_group if x["signal"] == "sigma_semantic" and not math.isnan(x["auroc"])),
        default=float("nan"),
    )
    best_lp = max(
        (x["auroc"] for x in per_group if x["signal"] == "sigma_logprob" and not math.isnan(x["auroc"])),
        default=float("nan"),
    )
    best_comb = max(
        (
            x["auroc"]
            for x in per_group
            if str(x["signal"]).startswith("sigma_combined") and not math.isnan(x["auroc"])
        ),
        default=float("nan"),
    )
    if not math.isnan(best_lp) and not math.isnan(best_sem) and best_lp - best_sem >= 0.05:
        conclusions.append(
            "Composition signal: logprob-only AUROC beats semantic-only by ≥0.05 "
            "(entropy-alone insufficient on this slice)."
        )
    if not math.isnan(best_comb) and not math.isnan(best_sem) and best_comb - best_sem >= 0.05:
        conclusions.append(
            "Composition signal: best weighted combined beats semantic-only by ≥0.05."
        )
    if not conclusions:
        conclusions.append(
            "No preset threshold fired: AUROC may stay near chance (~0.5) for this slice — "
            "treat σ as non-discriminative on this dataset under the tested arms, "
            "or expand prompts / models before any gate claim."
        )

    summary = {
        "title": "σ AUROC Rescue: Model Size, Sample Count, and Signal Composition Ablation",
        "best_overall": best_overall,
        "best_by_model": best_by_model,
        "per_group": per_group,
        "conclusions": conclusions,
        "interpretation_note": (
            "AUROC ≈ 0.51 is a diagnosis, not a catastrophe. If σ does not separate, "
            "the gate must not claim separation; if separation appears only with a larger "
            "model, more samples, or a different composition, record that as a boundary."
        ),
    }
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    args.summary.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"wrote {args.summary}")

    lines = [
        "# σ AUROC Rescue: Model Size, Sample Count, and Signal Composition Ablation",
        "",
        "*Diagnostic harness only — do not merge into flagship TruthfulQA headline claims without a repro bundle.*",
        "",
        "## Best overall (by AUROC)",
        "",
        "```json",
        json.dumps(best_overall, indent=2),
        "```",
        "",
        "## Conclusions (threshold rules)",
        "",
    ]
    for c in conclusions:
        lines.append(f"- {c}")
    lines.extend(["", "## Per (model, n_samples, signal)", "", "| model | n | signal | AUROC | AUPRC | ECE | acc\\|accept | abstain | wrong\\_conf | gap |", "|---|---:|---|---:|---:|---:|---:|---:|---:|---:|"])
    for x in sorted(per_group, key=lambda z: (-(z["auroc"] if not math.isnan(z["auroc"]) else -1), z["model"], z["n_samples"], z["signal"])):
        lines.append(
            f"| {x['model']} | {x['n_samples']} | `{x['signal']}` | {x['auroc']:.4f} | "
            f"{x['auprc']:.4f} | {x['ece']:.4f} | "
            f"{x['accuracy_accepted']:.3f} | {x['abstain_rate']:.3f} | {x['wrong_confident_count']} | "
            f"{x['sigma_gap']:.4f} |"
        )
    lines.append("")
    args.table.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.table}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
