#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""BENCH-2: drive SCI-6 row files through ``./cos-chat`` (BitNet bridge).

For each input row (from ``export_suite_hf_rows.py``) writes **two**
JSONL records — ``bitnet_only`` then ``pipeline`` — matching the
TruthfulQA detail layout so ``cos-bench-suite-sci`` can filter on
``mode: pipeline``.

Environment (required for real runs):

  CREATION_OS_BITNET_EXE
  CREATION_OS_BITNET_MODEL

Optional:

  COS_SUITE_REPO_ROOT   — default: parent of scripts/real twice
  COS_SUITE_MAX_ROWS    — cap rows per dataset (dev only)
  COS_SUITE_TIMEOUT_S   — per cos-chat invoke (default 180)
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, TextIO, Tuple

try:
    import fcntl

    _HAVE_FLOCK = True
except ImportError:  # pragma: no cover
    _HAVE_FLOCK = False

# ---------------------------------------------------------------------------
# Parsing cos-chat --once stdout (see src/cli/cos_chat.c run_chat_once).
# ---------------------------------------------------------------------------

def parse_round0(stdout: str) -> Tuple[Optional[str], Optional[float], Optional[str]]:
    """Return (generated_text, sigma_peak, action) from cos-chat --once output.

    When the model emits newlines, the C formatter still prints ``σ_peak`` on
    its own indented line (``\\n  [σ_peak=…]``) after the full response body.
    """
    s = stdout
    if "round 0" not in s:
        return None, None, None
    m = re.search(
        r"round 0\s+(.*?)\n  \["
        r"(?:\u03c3|σ)"
        r"_peak=([0-9.]+)\s+action=(\S+)\s+route=(\S+)\]",
        s,
        re.DOTALL,
    )
    if m:
        try:
            sig = float(m.group(2))
        except ValueError:
            sig = None
        return m.group(1).strip(), sig, m.group(3)
    # Single-line legacy shape (no embedded newlines in the response).
    for raw in s.splitlines():
        line = raw.strip()
        if not line.startswith("round 0"):
            continue
        for sep in ("[σ_peak=", "[\u03c3_peak="):
            if sep not in line:
                continue
            head, _, tail = line.partition(sep)
            if not head.startswith("round 0"):
                continue
            gen = head[len("round 0") :].strip()
            m2 = re.match(
                r"^([0-9.]+)\s+action=(\S+)\s+route=(\S+)\]", tail
            )
            if not m2:
                return gen, None, None
            try:
                sig2 = float(m2.group(1))
            except ValueError:
                sig2 = None
            return gen, sig2, m2.group(2)
    return None, None, None


# ---------------------------------------------------------------------------
# Graders (emit bool | null for JSONL ``correct`` — loader contract).
# ---------------------------------------------------------------------------

_MC_STANDALONE = re.compile(r"(?:^|[^A-Za-z])([A-Ea-e])(?:\)|\.|,|:|\s|$)")
_ANSWER_LETTER = re.compile(
    r"(?i)(?:\*\*answer\*\*|answer)\s*[:\.]?\s*([A-Ea-e])\b"
)


def sigma_heuristic(text: str) -> float:
    """Text-only σ fallback (matches ``run_truthfulqa_bitnet.sigma_heuristic``)."""
    t = (text or "").rstrip()
    n = len(t)
    if n == 0:
        return 1.0
    q = t.count("?")
    short_num = n <= 6 and any(ch.isdigit() for ch in t) and all(
        (ch.isdigit() or ch in " \t-+./") for ch in t
    )
    base = 0.18 + n * 0.00035
    if short_num:
        base *= 0.35
    base += 0.06 * q
    return max(0.05, min(0.95, base))


def grade_mc(generated: str, gold_letter: str) -> Optional[bool]:
    if not generated:
        return None
    gold = gold_letter.strip().upper()[:1]
    if not gold or gold not in "ABCDE":
        return None
    m = _ANSWER_LETTER.search(generated)
    if m:
        return m.group(1).upper() == gold
    hits = _MC_STANDALONE.findall(generated)
    if not hits:
        return None
    pred = hits[-1].upper()
    if pred not in "ABCDE":
        return None
    return pred == gold


_GSM_HASH = re.compile(r"####\s*(-?\d+)")


def grade_gsm8k(generated: str, gold: str) -> Optional[bool]:
    if not generated or not str(gold).strip():
        return None
    gold_s = str(gold).strip()
    m = _GSM_HASH.search(generated)
    if m:
        return m.group(1).lstrip("+") == gold_s.lstrip("+")
    return None


# ---------------------------------------------------------------------------
# cos-chat invocation
# ---------------------------------------------------------------------------


def build_argv(
    chat_bin: Path,
    mode: str,
    prompt: str,
    repo: Path,
    calib: Path,
) -> List[str]:
    base = [
        str(chat_bin),
        "--no-codex",
        "--local-only",
        "--once",
        "--no-transcript",
        "--no-icl",
        "--no-coherence",
        "--prompt",
        prompt,
    ]
    if mode == "bitnet_only":
        return [
            *base,
            "--no-conformal",
            "--tau-accept",
            "1.02",
            "--tau-rethink",
            "1.05",
        ]
    if mode == "pipeline":
        return [
            *base,
            "--conformal",
            "--calibration-path",
            str(calib),
        ]
    raise ValueError(mode)


def prepare_home(home: Path, calib_src: Path) -> None:
    dot = home / ".cos"
    dot.mkdir(parents=True, exist_ok=True)
    shutil.copy2(calib_src, dot / "calibration.json")


def run_one_fixed(
    argv: List[str],
    cwd: Path,
    env: Dict[str, str],
    timeout_s: int,
) -> Tuple[int, str, float]:
    t0 = time.monotonic()
    proc = subprocess.run(
        argv,
        cwd=str(cwd),
        env=env,
        stdin=subprocess.DEVNULL,
        capture_output=True,
        timeout=timeout_s,
        check=False,
    )
    out = (proc.stdout or b"").decode("utf-8", errors="replace")
    err = (proc.stderr or b"").decode("utf-8", errors="replace")
    dt = time.monotonic() - t0
    merged = out + ("\n" + err if err.strip() else "")
    return proc.returncode, merged, dt


def score_row(dataset: str, gen: str, gold: str) -> Optional[bool]:
    if dataset in ("arc_challenge", "arc_easy", "hellaswag"):
        return grade_mc(gen or "", gold)
    if dataset == "gsm8k":
        return grade_gsm8k(gen or "", gold)
    return None


def load_done_keys(path: Path) -> set:
    done = set()
    if not path.is_file():
        return done
    with path.open("r", encoding="utf-8") as fp:
        for line in fp:
            line = line.strip()
            if not line:
                continue
            try:
                o = json.loads(line)
            except json.JSONDecodeError:
                continue
            done.add((o.get("id"), o.get("mode")))
    return done


def dedupe_detail_jsonl(path: Path) -> int:
    """Remove duplicate ``(id, mode)`` rows; **last** line wins (resume-safe)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.is_file():
        return 0
    by_key: Dict[Tuple[Any, Any], str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        try:
            o = json.loads(line)
        except json.JSONDecodeError:
            continue
        by_key[(o.get("id"), o.get("mode"))] = line
    tmp = path.with_suffix(path.suffix + ".tmp")
    body = "\n".join(by_key.values()) + ("\n" if by_key else "")
    tmp.write_text(body, encoding="utf-8")
    os.replace(str(tmp), str(path))
    return len(by_key)


def append_line(fp: TextIO, obj: Dict[str, Any]) -> None:
    fp.write(json.dumps(obj, ensure_ascii=False) + "\n")
    fp.flush()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--rows",
        required=True,
        help="path to *_rows.jsonl (from export_suite_hf_rows.py)",
    )
    ap.add_argument(
        "--out",
        required=True,
        help="detail JSONL path (e.g. benchmarks/suite/arc_challenge_detail.jsonl)",
    )
    ap.add_argument(
        "--repo",
        default="",
        help="repo root (default: infer from this script)",
    )
    args = ap.parse_args()

    exe = os.environ.get("CREATION_OS_BITNET_EXE", "").strip()
    mdl = os.environ.get("CREATION_OS_BITNET_MODEL", "").strip()
    if not exe or not mdl:
        print(
            "run_suite_cos_chat_detail: set CREATION_OS_BITNET_EXE and "
            "CREATION_OS_BITNET_MODEL",
            file=sys.stderr,
        )
        return 2

    if args.repo:
        repo = Path(args.repo).resolve()
    else:
        repo = Path(__file__).resolve().parents[2]

    chat_bin = repo / "cos-chat"
    if not chat_bin.is_file() or not os.access(chat_bin, os.X_OK):
        print(f"run_suite_cos_chat_detail: missing executable {chat_bin}", file=sys.stderr)
        return 2

    calib_src = repo / "benchmarks" / "suite" / "bitnet_suite_calibration.json"
    if not calib_src.is_file():
        print(f"run_suite_cos_chat_detail: missing {calib_src}", file=sys.stderr)
        return 2

    timeout_s = int(os.environ.get("COS_SUITE_TIMEOUT_S", "180"))
    max_rows_env = os.environ.get("COS_SUITE_MAX_ROWS", "").strip()
    max_rows = int(max_rows_env) if max_rows_env.isdigit() else None

    rows_path = Path(args.rows).resolve()
    out_path = Path(args.out).resolve()

    dedupe_detail_jsonl(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_fp = out_path.open("a+", encoding="utf-8")
    if _HAVE_FLOCK:
        try:
            fcntl.flock(out_fp.fileno(), fcntl.LOCK_EX)
        except OSError:
            pass

    try:
        done = load_done_keys(out_path)
        with tempfile.TemporaryDirectory(prefix="cos_suite_home_") as tmp:
            home = Path(tmp)
            prepare_home(home, calib_src)
            base_env = os.environ.copy()
            base_env["HOME"] = str(home)
            base_env["COS_ENGRAM_DISABLE"] = "1"
            base_env["CREATION_OS_BITNET_EXE"] = exe
            base_env["CREATION_OS_BITNET_MODEL"] = mdl

            with rows_path.open("r", encoding="utf-8") as fp:
                n_rows = 0
                for row_idx, line in enumerate(fp):
                    if max_rows is not None and row_idx >= max_rows:
                        break
                    line = line.strip()
                    if not line:
                        continue
                    row = json.loads(line)
                    n_rows += 1
                    rid = row.get("id")
                    dataset = row.get("dataset", "")
                    prompt = str(row.get("prompt", "")).strip()
                    gold = str(row.get("gold", "")).strip()
                    for mode in ("bitnet_only", "pipeline"):
                        if (rid, mode) in done:
                            continue
                        argv = build_argv(chat_bin, mode, prompt, repo, calib_src)
                        rc, merged, wall_s = run_one_fixed(
                            argv, repo, base_env, timeout_s
                        )
                        gen, sigma, action = parse_round0(merged)
                        if gen and prompt and gen.startswith(prompt):
                            gen = gen[len(prompt) :].lstrip()
                        if sigma is None and gen is not None:
                            sigma = sigma_heuristic(gen)
                        elif sigma is None:
                            sigma = 0.5
                        abstained = bool(
                            gen
                            and (
                                "uncertain and cannot answer" in gen
                                or (action or "").upper() == "ABSTAIN"
                            )
                        )
                        esc = "ESCALATE" in merged and mode == "pipeline"
                        cor: Any = score_row(str(dataset), gen or "", gold)
                        if abstained and cor is None:
                            cor = None
                        rec: Dict[str, Any] = {
                            "id": rid,
                            "dataset": dataset,
                            "prompt": prompt,
                            "gold": gold,
                            "generated": gen if gen is not None else "",
                            "sigma": sigma if sigma is not None else 0.5,
                            "rounds": 1,
                            "escalated": bool(esc),
                            "abstained": bool(abstained),
                            "wall_s": round(wall_s, 4),
                            "gen_s": round(wall_s, 4),
                            "eur": 0.0001,
                            "correct": cor,
                            "mode": mode,
                            "cos_chat_rc": rc,
                        }
                        append_line(out_fp, rec)
                        done.add((rid, mode))
            print(
                json.dumps(
                    {"rows_file": str(rows_path), "out": str(out_path), "rows": n_rows},
                    indent=2,
                )
            )
    finally:
        if _HAVE_FLOCK:
            try:
                fcntl.flock(out_fp.fileno(), fcntl.LOCK_UN)
            except OSError:
                pass
        out_fp.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
