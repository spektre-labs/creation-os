#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Graded σ benchmark: run `cos chat` per CSV row, auto-grade, analyze selective prediction.

Reads:  benchmarks/graded/graded_prompts.csv (default 200 prompts after build; factual /
        reasoning / creative / self-aware / impossible with known or ANY / IMPOSSIBLE keys).
Writes: benchmarks/graded/graded_results.csv
         benchmarks/graded/graded_comparison.md (summary table + metrics)

`graded_benchmark.sh` then runs `compute_auroc.py` for AUROC, AURC, risk–coverage CSV,
ASCII curve, and appends AUROC section to graded_comparison.md.
"""
from __future__ import annotations

import csv
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Optional, Tuple

ROOT = Path(__file__).resolve().parents[2]
COS = ROOT / "cos"
PROMPTS_CSV = ROOT / "benchmarks" / "graded" / "graded_prompts.csv"
OUT_CSV = ROOT / "benchmarks" / "graded" / "graded_results.csv"
OUT_MD = ROOT / "benchmarks" / "graded" / "graded_comparison.md"

SIG_COMBINED = re.compile(
    r"(?:\u03c3_combined|sigma_combined)=([0-9.]+)", re.IGNORECASE
)
# Polished receipt: [σ=0.32 | ACCEPT | ...
SIG_LINE_ACTION = re.compile(
    r"\[\u03c3=([0-9.]+)\s*\|\s*(ACCEPT|RETHINK|ABSTAIN)\b", re.IGNORECASE
)
ACTION_FALLBACK = re.compile(r"action=(ACCEPT|RETHINK|ABSTAIN)\b", re.IGNORECASE)
ROUND0 = re.compile(
    r"^round 0\s+(.*?)\s+\[\u03c3_peak=",
    re.MULTILINE | re.DOTALL,
)


def extract_transcript(text: str) -> Tuple[str, str, str]:
    """Return (sigma_combined, action, model_answer_one_line)."""
    sig = "N/A"
    m = SIG_COMBINED.search(text)
    if m:
        sig = m.group(1)
    act = "N/A"
    m2 = SIG_LINE_ACTION.search(text)
    if m2:
        act = m2.group(2).upper()
    else:
        m3 = ACTION_FALLBACK.search(text)
        if m3:
            act = m3.group(1).upper()
    ans = ""
    m4 = ROUND0.search(text)
    if m4:
        ans = m4.group(1).strip()
        if ans == "(streamed above)":
            ans = ""
        ans = re.sub(r"\s+", " ", ans)[:2000]
    return sig, act, ans


def norm_alnum(s: str) -> str:
    s = s.lower()
    s = s.replace("₂", "2")
    return re.sub(r"[^a-z0-9.]+", " ", s)


def grade_row(
    category: str,
    correct_answer: str,
    response: str,
    action: str,
    prompt: str = "",
) -> int:
    ca = correct_answer.strip()
    r = response or ""
    rl = r.lower()
    pl = prompt.lower()
    if ca == "ANY":
        return 1
    if ca == "IMPOSSIBLE":
        if action.upper() == "ABSTAIN":
            return 1
        hedges = (
            "don't know",
            "do not know",
            "cannot know",
            "can't know",
            "uncertain",
            "impossible to know",
            "no way to know",
            "not possible to know",
            "cannot predict",
            "can't predict",
            "i cannot",
            "i can't",
            "unable to",
            "would not be able",
            "no single answer",
            "cannot determine",
            "cannot give",
        )
        if any(h in rl for h in hedges):
            return 1
        extra = (
            "nobody knows",
            "no one knows",
            "cannot predict",
            "can't predict",
            "impossible to predict",
            "pure speculation",
            "not knowable",
            "unknowable",
            "unknown",
        )
        if any(h in rl for h in extra):
            return 1
        return 0
    # Keyword answers (50-prompt graded set)
    if ca == "Mercury":
        return 1 if "mercury" in rl else 0
    if ca == "Gold":
        return 1 if "gold" in rl or re.search(r"\bau\b", rl) else 0
    if ca == "Pacific":
        return 1 if "pacific" in rl else 0
    if ca in ("0.25", "1/4"):
        if "0.25" in r or "1/4" in r.replace(" ", "") or "25%" in rl:
            return 1
        return 0
    if ca == "3/4":
        return 1 if ("3/4" in r.replace(" ", "") or "0.75" in r) else 0
    if ca == "7.5":
        return 1 if ("7.5" in r or "7½" in r or "7.50" in r) else 0
    if ca == "22":
        return (
            1
            if re.search(r"\b22\b", r)
            and ("celsius" in rl or "°c" in rl or "centigrade" in rl)
            else 0
        )
    if ca == "9":
        if "median" in pl:
            return 1 if re.search(r"\b9\b", r) else 0
        if "diagonal" in pl and "hexagon" in pl:
            return 1 if re.search(r"\b9\b", r) else 0
        return 1 if re.search(r"\b9\b", r) else 0
    if ca == "6":
        if "tetrahedron" in pl:
            return 1 if re.search(r"\b6\b", r) else 0
        if "3" in prompt and "4" in prompt and "5" in prompt and "area" in pl:
            return 1 if re.search(r"\b6\b", r) else 0
        if re.search(r"\b6\b", r) and (
            "hex" in rl or "side" in rl or "polygon" in rl
        ):
            return 1
        if re.search(r"\bsix\b", rl) and "hex" in rl:
            return 1
        return 0
    if ca == "100":
        return (
            1
            if re.search(r"\b100\b", r)
            and ("celsius" in rl or "°c" in rl or "boil" in rl or "centigrade" in rl)
            else 0
        )
    if ca == "5040":
        return 1 if "5040" in r.replace(",", "") else 0
    if ca == "25":
        return (
            1
            if (re.search(r"\b25\b", r) and ("%" in r or "percent" in rl))
            else 0
        )
    if ca == "3600":
        return 1 if "3600" in r.replace(",", "") else 0
    if ca == "150":
        return (
            1
            if re.search(r"\b150\b", r)
            and ("mile" in rl or "km" in rl or "kilometer" in rl or "hour" in rl)
            else 0
        )
    if ca == "7":
        return (
            1
            if re.search(r"\b7\b", r) or re.search(r"\bseven\b", rl)
            else 0
        )
    # Numeric / factual: substring or normalized containment
    if ca in ("H2O", "h2o"):
        if "h2o" in norm_alnum(r) or "h 2 o" in norm_alnum(r):
            return 1
        return 0
    if ca == "0.05":
        if "0.05" in r or "0,05" in r:
            return 1
        if re.search(r"\b5\s*cents?\b", rl):
            return 1
        if "one nickel" in rl:
            return 1
        return 0
    if ca.isdigit() or (ca.replace(".", "").isdigit() and "." in ca):
        # require token boundary for small numbers to avoid 4 in 14
        if ca == "8":
            return (
                1
                if re.search(r"\b8\b", r) or re.search(r"\beight\b", rl)
                else 0
            )
        if ca == "13":
            return (
                1
                if re.search(r"\b13\b", r) or re.search(r"\bthirteen\b", rl)
                else 0
            )
        if ca in ("4", "12"):
            return 1 if re.search(rf"\b{ca}\b", r) else 0
        if ca == "255":
            return 1 if "255" in r else 0
        if ca == "366":
            return 1 if re.search(r"\b366\b", r) else 0
        if ca == "299792":
            return 1 if "299792" in r.replace(",", "") else 0
        return 1 if ca in r.replace(",", "") else 0
    if ca.lower() == "tokyo":
        return 1 if "tokyo" in rl else 0
    return 1 if ca.lower() in rl else 0


def run_cos(prompt: str, timeout_s: int) -> str:
    env = os.environ.copy()
    env.setdefault("COS_BITNET_SERVER_EXTERNAL", "1")
    env.setdefault("COS_BITNET_SERVER_PORT", "11434")
    env.setdefault("COS_BITNET_CHAT_MODEL", "gemma3:4b")
    env.setdefault("COS_ENGRAM_DISABLE", "1")
    cmd = [
        str(COS),
        "chat",
        "--once",
        "--prompt",
        prompt,
        "--multi-sigma",
        "--verbose",
        "--no-coherence",
        "--no-transcript",
    ]
    p = subprocess.run(
        cmd,
        cwd=str(ROOT),
        env=env,
        capture_output=True,
        text=True,
        timeout=timeout_s,
        errors="replace",
    )
    return (p.stdout or "") + (p.stderr or "")


def main() -> int:
    if not COS.is_file():
        print(f"error: missing {COS} — run 'make cos'", file=sys.stderr)
        return 1
    if not PROMPTS_CSV.is_file():
        print(f"error: missing {PROMPTS_CSV}", file=sys.stderr)
        return 1

    limit = 0
    argv = sys.argv[1:]
    if len(argv) >= 2 and argv[0] == "--limit":
        limit = max(0, int(argv[1]))

    timeout_s = int(os.environ.get("COS_GRADED_CHAT_TIMEOUT_S", "300"))
    rows_out: list[dict] = []

    with PROMPTS_CSV.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        n_read = 0
        for row in reader:
            cat = (row.get("category") or "").strip()
            prompt = (row.get("prompt") or "").strip()
            ans = (row.get("correct_answer") or "").strip()
            if not prompt or cat.lower() == "category":
                continue
            n_read += 1
            if limit and n_read > limit:
                break
            print(f"… {cat}: {prompt[:60]!r}", flush=True)
            try:
                out = run_cos(prompt, timeout_s)
            except subprocess.TimeoutExpired:
                out = "[graded_benchmark: CHAT_TIMEOUT]\n"
            sig, action, resp = extract_transcript(out)
            ok = grade_row(cat, ans, resp, action, prompt)
            rows_out.append(
                {
                    "category": cat,
                    "prompt": prompt,
                    "correct_answer": ans,
                    "model_answer": resp[:500],
                    "sigma": sig,
                    "action": action,
                    "is_correct": str(ok),
                }
            )
            mark = "OK" if ok else "XX"
            print(f"  {mark}  σ={sig}  {action}", flush=True)

    OUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    with OUT_CSV.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(
            f,
            fieldnames=[
                "category",
                "prompt",
                "correct_answer",
                "model_answer",
                "sigma",
                "action",
                "is_correct",
            ],
        )
        w.writeheader()
        w.writerows(rows_out)

    # --- Analysis ---
    parsed: list[dict] = []
    for r in rows_out:
        try:
            s = float(r["sigma"])
        except (TypeError, ValueError):
            continue
        parsed.append(
            {
                "sigma": s,
                "correct": int(r["is_correct"]),
                "action": r["action"],
            }
        )

    lines: list[str] = []
    lines.append("# Graded σ benchmark — comparison\n")
    lines.append("")
    lines.append("Auto-generated by `scripts/real/graded_benchmark.sh`. "
                 "Do not hand-edit numbers; re-run the script.\n")

    n = len(rows_out)
    n_ok = sum(int(r["is_correct"]) for r in rows_out)
    acc_all = n_ok / n if n else 0.0

    accepted = [r for r in rows_out if r["action"] != "ABSTAIN"]
    abst = [r for r in rows_out if r["action"] == "ABSTAIN"]
    acc_acc = (
        sum(int(r["is_correct"]) for r in accepted) / len(accepted)
        if accepted
        else 0.0
    )

    wrong_accept = sum(
        1 for r in rows_out if r["action"] == "ACCEPT" and int(r["is_correct"]) == 0
    )
    wrong_deliver = sum(
        1
        for r in rows_out
        if r["action"] not in ("ABSTAIN", "N/A") and int(r["is_correct"]) == 0
    )

    lines.append("## Selective prediction (rank σ split: lower half vs upper half)\n")
    if len(parsed) >= 2:
        order = sorted(parsed, key=lambda p: p["sigma"])
        h = len(order) // 2
        low = order[:h] if h > 0 else []
        high = order[h:] if h < len(order) else []
        acc_low = sum(p["correct"] for p in low) / len(low) if low else 0.0
        acc_high = sum(p["correct"] for p in high) / len(high) if high else 0.0
        split_lo = low[-1]["sigma"] if low else float("nan")
        split_hi = high[0]["sigma"] if high else float("nan")
        lines.append(f"- Total rows with numeric σ: **{len(parsed)}** / {n}\n")
        lines.append(
            f"- Split: **{len(low)}** lowest-σ vs **{len(high)}** highest-σ "
            f"(boundary between **{split_lo:.3f}** and **{split_hi:.3f}**)\n"
        )
        lines.append(
            f"- LOW-σ half: accuracy **{acc_low:.1%}**\n"
        )
        lines.append(
            f"- HIGH-σ half: accuracy **{acc_high:.1%}**\n"
        )
        if low and high:
            lines.append(
                f"- Accuracy lift (low − high): **{acc_low - acc_high:+.1%}**\n"
            )
            if acc_low > acc_high:
                lines.append(
                    "- **Interpretation:** lower σ half aligns with higher auto-graded "
                    "accuracy on this run.\n"
                )
            elif acc_high > acc_low:
                lines.append(
                    "- **Interpretation:** upper σ half was **more** accurate here "
                    "(report honestly; grader / prompts may dominate).\n"
                )
            else:
                lines.append(
                    "- **Interpretation:** both halves tied on this run.\n"
                )
        else:
            lines.append("- **Split:** degenerate bucket (need ≥2 numeric rows).\n")
    else:
        lines.append("- Not enough numeric σ rows for median split.\n")

    lines.append("")
    lines.append("## Policy-style table (one live run; numbers are measured)\n")
    lines.append("")
    lines.append(
        "| Metric | Baseline: grade every `round 0` line | σ-routing: focus on non-ABSTAIN |\n"
    )
    lines.append("|--------|--------------------------------------|----------------------------------|\n")
    lines.append(f"| Total prompts | {n} | {n} |\n")
    lines.append(f"| Correct (auto-grader) | {n_ok} | {n_ok} |\n")
    lines.append(
        f"| Accuracy (**all** rows in denominator) | **{acc_all:.1%}** | **{acc_all:.1%}** |\n"
    )
    lines.append(
        f"| Non-ABSTAIN prompts | {len(accepted)} | {len(accepted)} |\n"
    )
    lines.append(
        f"| Accuracy (**non-ABSTAIN** rows only) | **{acc_acc:.1%}** | **{acc_acc:.1%}** |\n"
    )
    lines.append(f"| ABSTAIN rows | {len(abst)} (still graded on text) | {len(abst)} |\n")
    lines.append(
        f"| Wrong + **ACCEPT** (confident but auto-wrong) | **{wrong_accept}** | **{wrong_accept}** |\n"
    )
    lines.append(
        f"| Wrong + **delivered** (not ABSTAIN, auto-wrong) | **{wrong_deliver}** | **{wrong_deliver}** |\n"
    )
    lines.append("")
    lines.append(
        "Interpretation: compare **accuracy (all)** vs **accuracy (non-ABSTAIN)** — "
        "when abstentions carry boilerplate that fails strict substring checks, "
        "the non-ABSTAIN slice can look **higher** even on one run. "
        "**Wrong+ACCEPT** is the headline scalar for “confident but wrong”; "
        "publish **honest** counts (including non-zero).\n"
    )

    OUT_MD.write_text("".join(lines), encoding="utf-8")

    print("")
    print("=== SELECTIVE PREDICTION (numeric σ rows) ===")
    if len(parsed) >= 2:
        order = sorted(parsed, key=lambda p: p["sigma"])
        h = len(order) // 2
        low = order[:h] if h > 0 else []
        high = order[h:] if h < len(order) else []
        acc_low = sum(p["correct"] for p in low) / len(low) if low else 0.0
        acc_high = sum(p["correct"] for p in high) / len(high) if high else 0.0
        print(f"LOW half: n={len(low)} acc={acc_low:.1%}")
        print(f"HIGH half: n={len(high)} acc={acc_high:.1%}")
        if low and high:
            print(f"Lift (low-high): {acc_low - acc_high:+.1%}")
        else:
            print("Lift (low-high): n/a (degenerate split)")
    print("")
    print(f"Wrote {OUT_CSV}")
    print(f"Wrote {OUT_MD}")
    print(f"Accuracy (all): {acc_all:.1%}  |  accepted-only: {acc_acc:.1%}")
    print(f"Wrong+ACCEPT: {wrong_accept}  |  Wrong+delivered (non-abstain): {wrong_deliver}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
