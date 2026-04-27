#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""σ AUROC rescue ablation runner (diagnostic HTTP path; see results/README.md)."""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
RESULTS_DIR = HERE / "results"


def normalize_tokens(text: str) -> set[str]:
    s = (text or "").lower()
    s = re.sub(r"[^a-z0-9\s]+", " ", s)
    return {w for w in s.split() if w}


def jaccard(a: set[str], b: set[str]) -> float:
    if not a and not b:
        return 1.0
    if not a or not b:
        return 0.0
    inter = len(a & b)
    union = len(a | b)
    return inter / union if union else 0.0


def grade_one(response: str, best: str, worst: str) -> int:
    ra = normalize_tokens(response)
    sb = normalize_tokens(best)
    sw = normalize_tokens(worst)
    jc = jaccard(ra, sb)
    ji = jaccard(ra, sw)
    return 1 if jc > ji else 0


def first_sentence(text: str, cap: int = 512) -> str:
    t = (text or "").strip().replace("\r", " ")
    if not t:
        return ""
    cut = len(t)
    for sep in (".", "!", "?", "\n"):
        i = t.find(sep)
        if i >= 0 and i < cut:
            cut = i + 1
    t = t[: min(cut, cap)].strip()
    return t


def semantic_sigma_from_texts(texts: List[str]) -> float:
    """Match cos_chat ``chat_multi_shadow`` pairwise Jaccard → 1−mean_sim with clamps."""
    fs = [first_sentence(t) for t in texts if (t or "").strip()]
    if len(fs) < 2:
        return 0.5
    sims: List[float] = []
    for i in range(len(fs)):
        for j in range(i + 1, len(fs)):
            sims.append(cos_text_jaccard_fs(fs[i], fs[j]))
    mean_sim = sum(sims) / max(len(sims), 1)
    sigma_sem = 1.0 - mean_sim
    if mean_sim > 0.95:
        sigma_sem = 0.0
    elif mean_sim < 0.05:
        sigma_sem = 0.95
    return max(0.0, min(1.0, float(sigma_sem)))


def cos_text_jaccard_fs(a: str, b: str) -> float:
    """Word-bag Jaccard on whitespace-split tokens (cos_text_jaccard style, simplified)."""
    wa = re.findall(r"[A-Za-z0-9']+", (a or "").lower())
    wb = re.findall(r"[A-Za-z0-9']+", (b or "").lower())
    sa, sb = set(wa), set(wb)
    return jaccard(sa, sb)


def logprob_sigma_from_choice(choice: Dict[str, Any]) -> Optional[float]:
    """Mean normalized token entropy from OpenAI-shaped ``logprobs.content``."""
    lp = choice.get("logprobs")
    if not lp or not isinstance(lp, dict):
        return None
    content = lp.get("content")
    if not isinstance(content, list) or not content:
        return None
    hs: List[float] = []
    for tok in content:
        if not isinstance(tok, dict):
            continue
        tops = tok.get("top_logprobs")
        if not isinstance(tops, list) or len(tops) < 2:
            continue
        logits: List[float] = []
        for item in tops:
            if isinstance(item, dict):
                for _k, v in item.items():
                    if isinstance(v, (int, float)):
                        logits.append(float(v))
                    break
            elif isinstance(item, (int, float)):
                logits.append(float(item))
        if len(logits) < 2:
            continue
        m = max(logits)
        z = sum(math.exp(x - m) for x in logits)
        if z <= 0:
            continue
        probs = [math.exp(x - m) / z for x in logits]
        h = -sum(p * math.log(p + 1e-30) for p in probs if p > 0)
        hmax = math.log(len(logits))
        hs.append(h / hmax if hmax > 0 else 0.0)
    if not hs:
        return None
    return max(0.0, min(1.0, float(sum(hs) / len(hs))))


def ollama_chat_openai(
    host: str,
    port: int,
    model: str,
    user_text: str,
    temperature: float,
    max_tokens: int,
) -> Tuple[str, Optional[float], Dict[str, Any]]:
    url = f"http://{host}:{port}/v1/chat/completions"
    timeout = float(os.environ.get("COS_ABLATION_HTTP_TIMEOUT", "600"))
    last_exc: Optional[BaseException] = None
    for use_logprobs in (True, False):
        body: Dict[str, Any] = {
            "model": model,
            "messages": [{"role": "user", "content": user_text}],
            "temperature": float(temperature),
            "max_tokens": int(max_tokens),
            "stream": False,
        }
        if use_logprobs:
            body["logprobs"] = True
            body["top_logprobs"] = 5
        data = json.dumps(body).encode("utf-8")
        req = urllib.request.Request(
            url,
            data=data,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                raw = json.loads(resp.read().decode("utf-8"))
        except urllib.error.HTTPError as e:
            last_exc = e
            if use_logprobs and int(getattr(e, "code", 0) or 0) in (500, 502, 503):
                continue
            raise
        except (urllib.error.URLError, TimeoutError, OSError, json.JSONDecodeError) as e:
            last_exc = e
            if use_logprobs:
                continue
            raise
        ch0 = (raw.get("choices") or [{}])[0]
        msg = ch0.get("message") or {}
        text = str(msg.get("content") or "")
        s_lp = logprob_sigma_from_choice(ch0) if use_logprobs else None
        return text, s_lp, raw
    assert last_exc is not None
    raise last_exc


def ollama_n_samples(
    host: str,
    port: int,
    model: str,
    prompt: str,
    temperature: float,
    max_tokens: int,
    n: int,
    sequential: bool = True,
) -> Tuple[List[str], Optional[float]]:
    """Return completion texts and **mean** logprob-σ across samples (None if all missing)."""
    texts: List[str] = []
    lps: List[float] = []
    for i in range(n):
        t, lp, _ = ollama_chat_openai(host, port, model, prompt, temperature, max_tokens)
        texts.append(t)
        if lp is not None:
            lps.append(float(lp))
        if sequential:
            time.sleep(0.05)
    mean_lp: Optional[float] = None
    if lps:
        mean_lp = max(0.0, min(1.0, float(sum(lps) / len(lps))))
    return texts, mean_lp


def cos_fast_sigma(cos_bin: Path, prompt: str, extra_env: Dict[str, str]) -> Tuple[str, float, str]:
    env = dict(os.environ)
    env.update(extra_env)
    cmd = [str(cos_bin), "chat", "--once", "--json", "--no-stream", "--fast", "--no-cache", "--prompt", prompt]
    p = subprocess.run(
        cmd,
        cwd=str(REPO_ROOT),
        capture_output=True,
        text=True,
        timeout=int(os.environ.get("COS_ABLATION_COS_TIMEOUT", "600")),
        env=env,
    )
    err = (p.stderr or "")[-4000:]
    out = (p.stdout or "").strip().splitlines()
    js = None
    for line in reversed(out):
        line = line.strip()
        if line.startswith("{") and '"sigma"' in line:
            try:
                js = json.loads(line)
                break
            except json.JSONDecodeError:
                continue
    if js is None:
        return "", 1.0, err
    return str(js.get("response", "")), float(js.get("sigma", 1.0)), err


def load_prompts(path: Path, limit: int) -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []
    with path.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            rows.append(row)
            if len(rows) >= limit:
                break
    return rows


def emit_row(
    fp,
    *,
    pid: str,
    model_id: str,
    n_samples: int,
    temperature: float,
    signal: str,
    sigma: float,
    correct: bool,
    accepted: bool,
    answer: str,
    reference: str,
    sigma_logprob: Optional[float],
    sigma_semantic: float,
    w_sem: Optional[float],
    w_lp: Optional[float],
) -> None:
    rec = {
        "id": pid,
        "model": model_id,
        "n_samples": n_samples,
        "temperature": temperature,
        "signal": signal,
        "sigma": float(sigma),
        "correct": bool(correct),
        "accepted": bool(accepted),
        "answer": answer[:8000],
        "reference": reference[:4000],
        "sigma_logprob": None if sigma_logprob is None else float(sigma_logprob),
        "sigma_semantic": float(sigma_semantic),
        "weight_semantic": w_sem,
        "weight_logprob": w_lp,
    }
    fp.write(json.dumps(rec, ensure_ascii=False) + "\n")
    fp.flush()


def main() -> int:
    ap = argparse.ArgumentParser(description="σ ablation runner")
    ap.add_argument("--config", type=Path, default=HERE / "sigma_ablation_config.json")
    ap.add_argument("--limit", type=int, default=None, help="max prompts (default from config)")
    ap.add_argument("--quick", action="store_true", help="tiny smoke: 4 prompts, n=3, gemma only")
    ap.add_argument("--check", action="store_true", help="verify Ollama reachable and exit")
    ap.add_argument(
        "--full",
        action="store_true",
        help="use full_dataset_limit from config (0 = entire prompts CSV)",
    )
    ap.add_argument("--models", type=str, default=None, help="comma subset of model ids")
    args = ap.parse_args()

    cfg: Dict[str, Any] = json.loads(args.config.read_text(encoding="utf-8"))
    host = str(cfg.get("ollama_host", "127.0.0.1"))
    port = int(cfg.get("ollama_port", 11434))
    temperature = float(cfg.get("temperature", 0.7))
    tau = float(cfg.get("tau_accept", 0.3))
    max_tokens = int(cfg.get("max_tokens", 220))
    if os.environ.get("COS_ABLATION_MAX_TOKENS", "").strip().isdigit():
        max_tokens = int(os.environ["COS_ABLATION_MAX_TOKENS"].strip())
    n_list = [int(x) for x in cfg.get("n_samples_list", [3, 5, 8, 10])]
    weights: List[Tuple[float, float]] = [
        (float(a), float(b)) for a, b in cfg.get("weight_pairs_semantic_logprob", [])
    ]

    if args.check:
        try:
            urllib.request.urlopen(f"http://{host}:{port}/api/tags", timeout=3)
        except OSError as e:
            print(f"check failed: Ollama not reachable: {e}", file=sys.stderr)
            return 2
        t, lp, _ = ollama_chat_openai(host, port, "gemma3:4b", "ping", 0.1, 8)
        if not t and lp is None:
            print("check failed: empty completion", file=sys.stderr)
            return 2
        print("check-sigma-ablation: OK (Ollama HTTP)")
        return 0

    if args.limit is not None:
        limit = int(args.limit)
    elif args.full:
        fdl = cfg.get("full_dataset_limit", 0)
        if fdl is None or int(fdl) == 0:
            limit = 10**9
        else:
            limit = int(fdl)
    else:
        limit = int(cfg.get("prompt_limit_default", 100))
    if args.quick:
        limit = min(limit, 4)
        n_list = [3]
        weights = [(0.5, 0.5)]

    prompts_path = REPO_ROOT / str(cfg.get("prompts_csv", "benchmarks/truthfulqa200/prompts.csv"))
    if not prompts_path.is_file():
        print(f"error: missing prompts {prompts_path}", file=sys.stderr)
        return 2

    rows = load_prompts(prompts_path, limit)
    if not rows:
        print("error: no prompts", file=sys.stderr)
        return 2

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    detail_path = RESULTS_DIR / "sigma_ablation_detail.jsonl"
    fp = detail_path.open("w", encoding="utf-8")

    model_filter: Optional[set[str]] = None
    if args.models:
        model_filter = {x.strip() for x in args.models.split(",") if x.strip()}
    elif args.quick:
        model_filter = {"gemma3_4b"}

    for mspec in cfg.get("models", []):
        mid = str(mspec.get("id", ""))
        if model_filter is not None and mid not in model_filter:
            continue
        prov = str(mspec.get("provider", "ollama"))
        if prov == "ollama":
            omodel = str(mspec.get("ollama_model", "gemma3:4b"))
            if mspec.get("optional"):
                try:
                    urllib.request.urlopen(f"http://{host}:{port}/api/tags", timeout=3)
                    tags = json.loads(
                        urllib.request.urlopen(
                            f"http://{host}:{port}/api/tags", timeout=3
                        )
                        .read()
                        .decode()
                    )
                    names = {x.get("name", "") for x in (tags.get("models") or [])}
                    if omodel not in names and not any(
                        n.startswith(omodel.split(":")[0]) for n in names
                    ):
                        print(f"skip optional model {mid} ({omodel} not in ollama list)", file=sys.stderr)
                        continue
                except OSError:
                    print(f"skip optional model {mid} (no Ollama)", file=sys.stderr)
                    continue

            for n_samp in n_list:
                for row in rows:
                    pid = row["id"]
                    q = row.get("question", "")
                    best = row.get("best_answer", "")
                    worst = row.get("best_incorrect_answer", "")
                    ref = best[:2000]
                    print(f"  {mid} n={n_samp} {pid} …", flush=True)
                    try:
                        texts, s_lp = ollama_n_samples(
                            host, port, omodel, q, temperature, max_tokens, n_samp
                        )
                    except (urllib.error.URLError, TimeoutError, OSError) as e:
                        print(f"error HTTP {pid}: {e}", file=sys.stderr)
                        continue
                    s_sem = semantic_sigma_from_texts(texts)
                    primary = texts[0] if texts else ""
                    corr = bool(grade_one(primary, best, worst))
                    for w_sem, w_lp in weights:
                        if w_lp > 0.0 and s_lp is None:
                            continue
                        lp_use = float(s_lp) if s_lp is not None else 0.5
                        s_comb = max(0.0, min(1.0, w_sem * s_sem + w_lp * lp_use))
                        sig = f"sigma_combined_{w_sem:g}_{w_lp:g}"
                        accepted = s_comb < tau
                        emit_row(
                            fp,
                            pid=pid,
                            model_id=mid,
                            n_samples=n_samp,
                            temperature=temperature,
                            signal=sig,
                            sigma=s_comb,
                            correct=corr,
                            accepted=accepted,
                            answer=primary,
                            reference=ref,
                            sigma_logprob=s_lp,
                            sigma_semantic=s_sem,
                            w_sem=w_sem,
                            w_lp=w_lp,
                        )
                    if s_lp is not None:
                        acc_lp = float(s_lp) < tau
                        emit_row(
                            fp,
                            pid=pid,
                            model_id=mid,
                            n_samples=n_samp,
                            temperature=temperature,
                            signal="sigma_logprob",
                            sigma=float(s_lp),
                            correct=corr,
                            accepted=acc_lp,
                            answer=primary,
                            reference=ref,
                            sigma_logprob=s_lp,
                            sigma_semantic=s_sem,
                            w_sem=None,
                            w_lp=None,
                        )
                    acc_sem = s_sem < tau
                    emit_row(
                        fp,
                        pid=pid,
                        model_id=mid,
                        n_samples=n_samp,
                        temperature=temperature,
                        signal="sigma_semantic",
                        sigma=float(s_sem),
                        correct=corr,
                        accepted=acc_sem,
                        answer=primary,
                        reference=ref,
                        sigma_logprob=s_lp,
                        sigma_semantic=s_sem,
                        w_sem=None,
                        w_lp=None,
                    )

        elif prov == "cos_fast":
            cos_bin = REPO_ROOT / str(mspec.get("cos_bin", "cos"))
            if not cos_bin.is_file():
                if mspec.get("optional"):
                    print(f"skip optional {mid} (no cos binary)", file=sys.stderr)
                    continue
                print(f"error: missing {cos_bin}", file=sys.stderr)
                return 2
            extra = {}
            for n_samp in n_list:
                for row in rows:
                    pid = row["id"]
                    q = row.get("question", "")
                    best = row.get("best_answer", "")
                    worst = row.get("best_incorrect_answer", "")
                    ref = best[:2000]
                    ans, sig, _e = cos_fast_sigma(cos_bin, q, extra)
                    corr = bool(grade_one(ans, best, worst))
                    acc = sig < tau
                    emit_row(
                        fp,
                        pid=pid,
                        model_id=mid,
                        n_samples=n_samp,
                        temperature=temperature,
                        signal="sigma_cos_fast_scalar",
                        sigma=float(sig),
                        correct=corr,
                        accepted=acc,
                        answer=ans,
                        reference=ref,
                        sigma_logprob=None,
                        sigma_semantic=float(sig),
                        w_sem=None,
                        w_lp=None,
                    )
        else:
            print(f"unknown provider {prov}", file=sys.stderr)
            return 2

    fp.close()
    print(f"wrote → {detail_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
