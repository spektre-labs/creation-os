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
from typing import Any, DefaultDict, Dict, List, Optional, Tuple

HERE = Path(__file__).resolve().parent
RESULTS_DIR = HERE / "results"


def _row_ts(r: Dict[str, Any]) -> str:
    return str(r.get("temp_strategy") or "legacy")


def write_plots_best_group(rows: List[Dict[str, Any]], best: Dict[str, Any], out_dir: Path) -> None:
    """Optional ROC + σ histogram for the best (model, n_samples, signal, temp_strategy) arm."""
    if not best:
        return
    m, ns, sig = str(best["model"]), int(best["n_samples"]), str(best["signal"])
    tsb = str(best.get("temp_strategy", "legacy"))
    sub = [
        r
        for r in rows
        if str(r["model"]) == m
        and int(r["n_samples"]) == ns
        and str(r["signal"]) == sig
        and _row_ts(r) == tsb
    ]
    if len(sub) < 4:
        print("plots: skipped (fewer than 4 rows in best arm)", file=sys.stderr)
        return
    try:
        from sklearn.metrics import auc, roc_curve

        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as e:
        print(f"plots: skipped ({e})", file=sys.stderr)
        return

    y = [1 - int(x["correct"]) for x in sub]
    s = [float(x["sigma"]) for x in sub]
    if len(set(y)) < 2:
        print("plots: skipped (single class in best arm)", file=sys.stderr)
        return

    out_dir.mkdir(parents=True, exist_ok=True)
    fpr, tpr, _ = roc_curve(y, s)
    roc_auc = auc(fpr, tpr)
    plt.figure(figsize=(5, 4))
    plt.plot(fpr, tpr, label=f"AUROC={roc_auc:.3f}")
    plt.plot([0, 1], [0, 1], "k--", alpha=0.3)
    plt.xlabel("FPR")
    plt.ylabel("TPR")
    plt.title(f"ROC — {m} n={ns} `{sig}` [{tsb}]")
    plt.legend(loc="lower right")
    plt.tight_layout()
    p_roc = out_dir / "sigma_roc.png"
    plt.savefig(p_roc, dpi=120)
    plt.close()

    sc = [float(x["sigma"]) for x in sub if x["correct"]]
    sw = [float(x["sigma"]) for x in sub if not x["correct"]]
    plt.figure(figsize=(5, 4))
    if sc:
        plt.hist(sc, bins=12, alpha=0.55, label="correct", density=True)
    if sw:
        plt.hist(sw, bins=12, alpha=0.55, label="wrong", density=True)
    plt.xlabel("σ")
    plt.ylabel("density")
    plt.title(f"σ — {m} n={ns} [{tsb}]")
    plt.legend()
    plt.tight_layout()
    p_hist = out_dir / "sigma_hist_correct_wrong.png"
    plt.savefig(p_hist, dpi=120)
    plt.close()
    print(f"wrote {p_roc} and {p_hist}")


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


def _finite_sigma(x: Dict[str, Any]) -> bool:
    try:
        v = float(x.get("sigma", float("nan")))
        return not (math.isnan(v) or math.isinf(v))
    except (TypeError, ValueError):
        return False


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--detail", type=Path, default=RESULTS_DIR / "sigma_ablation_detail.jsonl")
    ap.add_argument("--summary", type=Path, default=RESULTS_DIR / "sigma_ablation_summary.json")
    ap.add_argument("--table", type=Path, default=RESULTS_DIR / "sigma_ablation_table.md")
    ap.add_argument("--tau-wrong-conf", type=float, default=0.3)
    ap.add_argument(
        "--plots",
        action="store_true",
        help="write sigma_roc.png + sigma_hist_correct_wrong.png for best AUROC arm (needs matplotlib)",
    )
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

    groups: DefaultDict[Tuple[str, int, str, str], List[Dict[str, Any]]] = defaultdict(list)
    for r in rows:
        key = (str(r["model"]), int(r["n_samples"]), str(r["signal"]), _row_ts(r))
        groups[key].append(r)

    per_group: List[Dict[str, Any]] = []
    for (model, n_samp, signal, ts_key), recs_all in sorted(groups.items()):
        recs = [x for x in recs_all if _finite_sigma(x)]
        if not recs:
            continue
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
            if (not x["correct"]) and float(x["sigma"]) < float(args.tau_wrong_conf)
        )
        sc = [float(x["sigma"]) for x in recs if x["correct"]]
        sw = [float(x["sigma"]) for x in recs if not x["correct"]]
        m_c = sum(sc) / len(sc) if sc else float("nan")
        m_w = sum(sw) / len(sw) if sw else float("nan")
        gap = (m_w - m_c) if (sc and sw) else float("nan")
        viable = (not math.isnan(auroc) and auroc > 0.65)
        failed = (not math.isnan(auroc) and auroc < 0.54)
        per_group.append(
            {
                "model": model,
                "n_samples": n_samp,
                "temp_strategy": ts_key,
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
                "viable_auroc_above_0p65": bool(viable),
                "failed_auroc_below_0p54": bool(failed),
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

    def parse_combined_weights(sig: str) -> Optional[Tuple[float, float]]:
        s = str(sig)
        for prefix in ("sigma_combined_semantic_", "sigma_combined_seu_", "sigma_combined_"):
            if s.startswith(prefix):
                rest = s[len(prefix) :]
                parts = rest.split("_", 1)
                if len(parts) != 2:
                    return None
                try:
                    return float(parts[0]), float(parts[1])
                except ValueError:
                    return None
        return None

    def signal_family(sig: str) -> str:
        s = str(sig)
        if s == "sigma_semantic":
            return "semantic"
        if s == "sigma_seu":
            return "seu"
        if s == "sigma_logprob":
            return "logprob"
        if s.startswith("sigma_combined_semantic"):
            return "combined_semantic"
        if s.startswith("sigma_combined_seu"):
            return "combined_seu"
        if s.startswith("sigma_combined"):
            return "combined"
        return "other"

    # Near-chance σ_semantic: honest "failed" flag (no hidden optimism).
    semantic_failed: List[Dict[str, Any]] = []
    for x in per_group:
        if x["signal"] != "sigma_semantic" or math.isnan(float(x["auroc"])):
            continue
        a = float(x["auroc"])
        failed = 0.48 <= a <= 0.54
        semantic_failed.append(
            {
                "model": x["model"],
                "n_samples": x["n_samples"],
                "temp_strategy": x["temp_strategy"],
                "auroc": a,
                "sigma_semantic_failed_diagnostic": bool(failed),
            }
        )

    bo_sig = str(best_overall.get("signal", "")) if best_overall else ""
    cw = parse_combined_weights(bo_sig)
    best_report = {
        "best_signal": bo_sig,
        "best_signal_family": signal_family(bo_sig) if best_overall else "",
        "best_temp_strategy": str(best_overall.get("temp_strategy", "")) if best_overall else "",
        "best_weight_semantic_logprob": list(cw) if cw else None,
        "best_auroc": best_overall.get("auroc") if best_overall else float("nan"),
        "best_sigma_gap": best_overall.get("sigma_gap") if best_overall else float("nan"),
    }

    gem_best = best_by_model.get("gemma3_4b") or {}
    phi_best = best_by_model.get("phi4") or {}
    bit_best = best_by_model.get("bitnet_2b_current") or {}
    model_delta = {
        "gemma3_4b_minus_bitnet_auroc": (
            float(gem_best["auroc"]) - float(bit_best["auroc"])
            if gem_best and bit_best and not math.isnan(gem_best["auroc"]) and not math.isnan(bit_best["auroc"])
            else float("nan")
        ),
        "phi4_minus_bitnet_auroc": (
            float(phi_best["auroc"]) - float(bit_best["auroc"])
            if phi_best and bit_best and not math.isnan(phi_best["auroc"]) and not math.isnan(bit_best["auroc"])
            else float("nan")
        ),
    }

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
            if str(x["signal"]).startswith("sigma_combined_semantic") and not math.isnan(x["auroc"])
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

    flagged_semantic = [x for x in semantic_failed if x["sigma_semantic_failed_diagnostic"]]
    semantic_notes: List[str] = []
    if flagged_semantic:
        semantic_notes.append(
            "σ_semantic AUROC ∈ [0.48, 0.54] (mark as non-separating / failed diagnostic for that setup): "
            + "; ".join(
                f"{x['model']}|n={x['n_samples']}|{x['auroc']:.3f}" for x in flagged_semantic[:24]
            )
            + (" …" if len(flagged_semantic) > 24 else "")
        )

    threshold_conclusions = conclusions[:]
    if not threshold_conclusions and not semantic_notes:
        threshold_conclusions.append(
            "No preset threshold fired: AUROC may stay near chance (~0.5) for this slice — "
            "treat σ as non-discriminative on this dataset under the tested arms, "
            "or expand prompts / models before any gate claim."
        )
    conclusions = threshold_conclusions + semantic_notes

    def max_auroc(pred) -> float:
        xs = [float(x["auroc"]) for x in per_group if pred(x) and not math.isnan(float(x["auroc"]))]
        return max(xs) if xs else float("nan")

    mct_best = max_auroc(lambda x: "mct" in str(x["temp_strategy"]))
    fixed_best = max_auroc(lambda x: str(x["temp_strategy"]).startswith("fixed"))
    seu_best = max_auroc(lambda x: x["signal"] == "sigma_seu")
    sem_best = max_auroc(lambda x: x["signal"] == "sigma_semantic")
    mct_improves_ge_0p03 = bool(
        not math.isnan(mct_best) and not math.isnan(fixed_best) and (mct_best - fixed_best >= 0.03)
    )
    mct_improves_ge_0p05 = bool(
        not math.isnan(mct_best) and not math.isnan(fixed_best) and (mct_best - fixed_best >= 0.05)
    )
    mct_improves = mct_improves_ge_0p03

    seu_improves_ge_0p03 = bool(
        not math.isnan(seu_best) and not math.isnan(sem_best) and (seu_best - sem_best >= 0.03)
    )
    seu_improves_ge_0p05 = bool(
        not math.isnan(seu_best) and not math.isnan(sem_best) and (seu_best - sem_best >= 0.05)
    )
    seu_improves = seu_improves_ge_0p03

    best_single = max(
        max_auroc(lambda x: x["signal"] == "sigma_semantic"),
        max_auroc(lambda x: x["signal"] == "sigma_seu"),
        max_auroc(lambda x: x["signal"] == "sigma_logprob"),
    )
    best_combined_any = max_auroc(lambda x: str(x["signal"]).startswith("sigma_combined"))
    multi_improves = bool(
        not math.isnan(best_combined_any)
        and not math.isnan(best_single)
        and (best_combined_any - best_single >= 0.05)
    )

    auroc_above_065 = any(
        (not math.isnan(float(x["auroc"])) and float(x["auroc"]) > 0.65) for x in per_group
    )

    if auroc_above_065:
        recommendation = (
            "At least one (model, n, signal, temp_strategy) arm exceeds 0.65 AUROC — "
            "worth deeper validation; diagnostic only until a repro bundle exists."
        )
    elif seu_improves:
        recommendation = (
            "SEU (embedding) best AUROC exceeds Jaccard semantic by ≥0.03 — "
            "embedding channel may be more informative on this slice."
        )
    elif mct_improves:
        recommendation = (
            "MCT (random per-sample T) best AUROC exceeds fixed-T by ≥0.03 — "
            "temperature strategy may matter for separation."
        )
    elif multi_improves:
        recommendation = (
            "Best combined (semantic/SEU + logprob) AUROC exceeds best single-channel by ≥0.05 — "
            "multi-channel composition helps on this slice."
        )
    else:
        recommendation = (
            "No preset uplift vs chance in this summary — treat as boundary evidence; "
            "do not upgrade gate claims without stronger separation."
        )

    conclusions_structured: Dict[str, Any] = {
        "mct_vs_fixed": {
            "mct_best_auroc": mct_best,
            "fixed_best_auroc": fixed_best,
            "mct_improves": mct_improves,
            "mct_improves_ge_0p03": mct_improves_ge_0p03,
            "mct_improves_ge_0p05": mct_improves_ge_0p05,
            "delta_auroc": (
                (mct_best - fixed_best)
                if (not math.isnan(mct_best) and not math.isnan(fixed_best))
                else float("nan")
            ),
        },
        "seu_vs_semantic": {
            "seu_best_auroc": seu_best,
            "semantic_best_auroc": sem_best,
            "seu_improves": seu_improves,
            "seu_improves_ge_0p03": seu_improves_ge_0p03,
            "seu_improves_ge_0p05": seu_improves_ge_0p05,
            "delta_auroc": (
                (seu_best - sem_best)
                if (not math.isnan(seu_best) and not math.isnan(sem_best))
                else float("nan")
            ),
        },
        "combined_vs_single": {
            "combined_best_auroc": best_combined_any,
            "single_best_auroc": best_single,
            "combined_improves": multi_improves,
        },
        "best_overall": (
            {
                "signal": best_overall.get("signal"),
                "model": best_overall.get("model"),
                "n_samples": best_overall.get("n_samples"),
                "temp_strategy": best_overall.get("temp_strategy"),
                "auroc": best_overall.get("auroc"),
                "sigma_gap": best_overall.get("sigma_gap"),
            }
            if best_overall
            else {}
        ),
        "auroc_above_0p65_any_arm": bool(auroc_above_065),
        "auroc_above_0.65": bool(auroc_above_065),
        "recommendation": recommendation,
    }

    def json_sanitize(o: Any) -> Any:
        if isinstance(o, float) and (math.isnan(o) or math.isinf(o)):
            return None
        if isinstance(o, dict):
            return {str(k): json_sanitize(v) for k, v in o.items()}
        if isinstance(o, list):
            return [json_sanitize(v) for v in o]
        return o

    model_improves = (
        (not math.isnan(model_delta["gemma3_4b_minus_bitnet_auroc"]) and model_delta["gemma3_4b_minus_bitnet_auroc"] >= 0.10)
        or (not math.isnan(model_delta["phi4_minus_bitnet_auroc"]) and model_delta["phi4_minus_bitnet_auroc"] >= 0.10)
    )

    summary = {
        "title": "σ AUROC Rescue: Model Size, Sample Count, and Signal Composition Ablation",
        "best_overall": best_overall,
        "best_by_model": best_by_model,
        "best_signal_report": best_report,
        "model_auroc_deltas": model_delta,
        "model_change_improves_auroc_by_threshold_0p10": bool(model_improves),
        "sigma_semantic_near_chance_by_setup": semantic_failed,
        "per_group": per_group,
        "conclusions": conclusions,
        "conclusions_structured": conclusions_structured,
        "conclusions_dict": {
            "mct_vs_fixed": conclusions_structured["mct_vs_fixed"],
            "seu_vs_semantic": conclusions_structured["seu_vs_semantic"],
            "best_overall": conclusions_structured["best_overall"],
            "auroc_above_0.65": conclusions_structured["auroc_above_0.65"],
            "recommendation": recommendation,
        },
        "interpretation_note": (
            "AUROC ≈ 0.51 is a diagnosis, not a catastrophe. If σ does not separate, "
            "the gate must not claim separation; if separation appears only with a larger "
            "model, more samples, or a different composition, record that as a boundary."
        ),
    }
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    args.summary.write_text(json.dumps(json_sanitize(summary), indent=2), encoding="utf-8")
    print(f"wrote {args.summary}")

    if args.plots:
        write_plots_best_group(rows, best_overall, args.table.parent)

    def _fmt_md_num(v: Any, nd: int = 4) -> str:
        if not isinstance(v, (int, float)) or (isinstance(v, float) and (math.isnan(v) or math.isinf(v))):
            return "nan"
        return f"{float(v):.{nd}f}"

    base_sigs = ["sigma_logprob", "sigma_semantic", "sigma_seu"]
    models_sorted = sorted({x["model"] for x in per_group})

    def max_auroc_mst(model: str, signal: str, ts_ok) -> float:
        xs = [
            float(z["auroc"])
            for z in per_group
            if str(z["model"]) == model
            and str(z["signal"]) == signal
            and ts_ok(z)
            and not math.isnan(float(z["auroc"]))
        ]
        return max(xs) if xs else float("nan")

    v2_lines: List[str] = [
        "## Best configurations (AUROC > 0.60)",
        "",
        "| Rank | Model | Signal | n_samples | Temp | AUROC | σ_gap | Accuracy | Abstain% |",
        "|---:|---|---|---:|---|---:|---:|---:|---:|",
    ]
    over_060 = [
        x for x in per_group if not math.isnan(float(x["auroc"])) and float(x["auroc"]) > 0.60
    ]
    over_060.sort(
        key=lambda z: (
            -float(z["auroc"]),
            str(z["model"]),
            int(z["n_samples"]),
            str(z["temp_strategy"]),
            str(z["signal"]),
        )
    )
    if not over_060:
        v2_lines.append("| — | *(none above 0.60 in this run)* | | | | | | | |")
    else:
        for i, x in enumerate(over_060[:48], start=1):
            v2_lines.append(
                f"| {i} | {x['model']} | `{x['signal']}` | {x['n_samples']} | `{x['temp_strategy']}` | "
                f"{float(x['auroc']):.4f} | {_fmt_md_num(x['sigma_gap'])} | {_fmt_md_num(x['accuracy_accepted'])} | "
                f"{_fmt_md_num(x['abstain_rate'])} |"
            )

    v2_lines.extend(
        [
            "",
            "## MCT vs fixed temperature",
            "",
            "| Model | Signal | Fixed 0.5 AUROC | Fixed 0.7 AUROC | MCT AUROC | Winner |",
            "|---|---|---|---:|---:|---:|---|",
        ]
    )
    for m in models_sorted:
        for sig in base_sigs:
            a05 = max_auroc_mst(m, sig, lambda z: str(z["temp_strategy"]) == "fixed_0.5")
            a07 = max_auroc_mst(m, sig, lambda z: str(z["temp_strategy"]) == "fixed_0.7")
            am = max_auroc_mst(m, sig, lambda z: "mct" in str(z["temp_strategy"]).lower())
            trip = [("fixed_0.5", a05), ("fixed_0.7", a07), ("MCT", am)]
            winner = "—"
            bests = [(lab, v) for lab, v in trip if not math.isnan(v)]
            if bests:
                wlab, wv = max(bests, key=lambda t: t[1])
                winner = f"{wlab} ({wv:.4f})"
            v2_lines.append(
                f"| {m} | `{sig}` | {_fmt_md_num(a05)} | {_fmt_md_num(a07)} | {_fmt_md_num(am)} | {winner} |"
            )

    v2_lines.extend(
        [
            "",
            "## SEU vs semantic entropy",
            "",
            "| Model | n_samples | Temp | SE AUROC | SEU AUROC | Δ AUROC | Winner |",
            "|---|---:|---|---:|---:|---:|---|",
        ]
    )
    trip_keys = sorted(
        {(str(z["model"]), int(z["n_samples"]), str(z["temp_strategy"])) for z in per_group}
    )
    for m, ns, ts in trip_keys:
        if "cos_fast" in ts:
            continue
        a_se = max(
            (
                float(z["auroc"])
                for z in per_group
                if str(z["model"]) == m
                and int(z["n_samples"]) == ns
                and str(z["temp_strategy"]) == ts
                and str(z["signal"]) == "sigma_semantic"
                and not math.isnan(float(z["auroc"]))
            ),
            default=float("nan"),
        )
        a_seu = max(
            (
                float(z["auroc"])
                for z in per_group
                if str(z["model"]) == m
                and int(z["n_samples"]) == ns
                and str(z["temp_strategy"]) == ts
                and str(z["signal"]) == "sigma_seu"
                and not math.isnan(float(z["auroc"]))
            ),
            default=float("nan"),
        )
        if math.isnan(a_se) and math.isnan(a_seu):
            continue
        d = a_seu - a_se if (not math.isnan(a_se) and not math.isnan(a_seu)) else float("nan")
        win = "—"
        if not math.isnan(a_se) and not math.isnan(a_seu):
            if a_seu > a_se + 1e-9:
                win = "SEU"
            elif a_se > a_seu + 1e-9:
                win = "SE"
            else:
                win = "tie"
        v2_lines.append(
            f"| {m} | {ns} | `{ts}` | {_fmt_md_num(a_se)} | {_fmt_md_num(a_seu)} | {_fmt_md_num(d)} | {win} |"
        )

    v2_lines.extend(
        [
            "",
            "## Signal comparison (best AUROC per model, then averaged across models)",
            "",
            "| Signal | Mean AUROC | Mean σ_gap | Mean Accuracy |",
            "|---|---:|---:|---:|",
        ]
    )
    signals_sorted = sorted({str(x["signal"]) for x in per_group})
    for sig in signals_sorted:
        m_aucs: List[float] = []
        m_gaps: List[float] = []
        m_accs: List[float] = []
        for m in models_sorted:
            sub = [
                x
                for x in per_group
                if str(x["model"]) == m and str(x["signal"]) == sig and not math.isnan(float(x["auroc"]))
            ]
            if not sub:
                continue
            b = max(sub, key=lambda z: float(z["auroc"]))
            m_aucs.append(float(b["auroc"]))
            if not math.isnan(float(b["sigma_gap"])):
                m_gaps.append(float(b["sigma_gap"]))
            if not math.isnan(float(b["accuracy_accepted"])):
                m_accs.append(float(b["accuracy_accepted"]))
        if not m_aucs:
            continue
        ma = sum(m_aucs) / len(m_aucs)
        mg = sum(m_gaps) / len(m_gaps) if m_gaps else float("nan")
        mcc = sum(m_accs) / len(m_accs) if m_accs else float("nan")
        v2_lines.append(f"| `{sig}` | {_fmt_md_num(ma)} | {_fmt_md_num(mg)} | {_fmt_md_num(mcc)} |")

    mct_delta = (
        (mct_best - fixed_best)
        if (not math.isnan(mct_best) and not math.isnan(fixed_best))
        else float("nan")
    )
    seu_delta = (
        (seu_best - sem_best)
        if (not math.isnan(seu_best) and not math.isnan(sem_best))
        else float("nan")
    )
    best_au = (
        float(best_overall["auroc"])
        if best_overall and not math.isnan(float(best_overall["auroc"]))
        else float("nan")
    )
    v2_lines.extend(
        [
            "",
            "## Conclusions",
            "",
            f"- MCT improves AUROC (≥0.05): **{'YES' if mct_improves_ge_0p05 else 'NO'}** (Δ = {_fmt_md_num(mct_delta)})",
            f"- SEU improves over SE (≥0.05): **{'YES' if seu_improves_ge_0p05 else 'NO'}** (Δ = {_fmt_md_num(seu_delta)})",
            f"- Best AUROC achieved: {_fmt_md_num(best_au)}",
            f"- σ-gate viable (AUROC > 0.65 on some arm): **{'YES' if auroc_above_065 else 'NO'}**",
            f"- Recommendation: {recommendation}",
        ]
    )

    lines = [
        "# σ-Ablation v2 Results",
        "",
        "*Diagnostic harness only — do not merge into flagship TruthfulQA headline claims without a repro bundle.*",
        "",
    ]
    lines.extend(v2_lines)
    lines.extend(
        [
            "",
            "---",
            "",
            "## Historical / raw sections",
            "",
            "### Best overall (by AUROC)",
            "",
            "```json",
            json.dumps(json_sanitize(best_overall), indent=2),
            "```",
            "",
            "### Best signal / weights (parsed)",
            "",
            "```json",
            json.dumps(json_sanitize(best_report), indent=2),
            "```",
            "",
            "### Model AUROC deltas (best arm per model id)",
            "",
            "```json",
            json.dumps(json_sanitize(model_delta), indent=2),
            "```",
            "",
            "### σ_semantic near-chance (failed diagnostic) by setup",
            "",
            "AUROC ∈ [0.48, 0.54] ⇒ mark `sigma_semantic` as **non-separating** for that (model, n); not a hidden failure.",
            "",
            "```json",
            json.dumps(json_sanitize(flagged_semantic), indent=2),
            "```",
            "",
            "### Conclusions (threshold rules)",
            "",
        ]
    )
    for c in conclusions:
        lines.append(f"- {c}")
    lines.extend(
        [
            "",
            "## Structured conclusions (MCT / SEU / combined)",
            "",
            "```json",
            json.dumps(json_sanitize(conclusions_structured), indent=2),
            "```",
            "",
        ]
    )
    lines.extend(["", "## Per (model, n, temp_strategy, signal)", "", "| model | n | temp_strategy | signal | AUROC | viable | failed | gap |", "|---|---:|---|---|---:|---:|---:|---:|"])
    for x in sorted(
        per_group,
        key=lambda z: (
            -(z["auroc"] if not math.isnan(z["auroc"]) else -1),
            z["model"],
            z["n_samples"],
            str(z["temp_strategy"]),
            z["signal"],
        ),
    ):
        lines.append(
            f"| {x['model']} | {x['n_samples']} | `{x['temp_strategy']}` | `{x['signal']}` | "
            f"{x['auroc']:.4f} | {x['viable_auroc_above_0p65']} | {x['failed_auroc_below_0p54']} | "
            f"{_fmt_md_num(x['sigma_gap'])} |"
        )
    lines.append("")
    args.table.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.table}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
