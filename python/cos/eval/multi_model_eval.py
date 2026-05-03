# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Multi-model σ-gate evaluation: same detector, many generators (OpenAI-compatible ``/v1``).

**Never** store Hugging Face or cloud tokens in source; use ``HF_TOKEN`` / ``HUGGINGFACE_HUB_TOKEN``
only on the host for local transformers runs (see ``gemma_eval.py``).

Run (example)::

    cos bench --multi-model --models gemma3-4b,qwen3.6-a3b --n 30 --dataset truthfulqa

This answers: *does σ separate this model’s correct vs incorrect generations on a fixed slice?*
It does **not** rank LM capability — claim discipline: [docs/CLAIM_DISCIPLINE.md](../../docs/CLAIM_DISCIPLINE.md).
"""
from __future__ import annotations

import json
import math
import re
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Callable, Dict, List, Mapping, Optional, Sequence, Tuple, Union

from cos.bench import HALUEVAL_EXAMPLE_AUROC
from cos.eval.checkpoint_eval import CheckpointEval
from cos.eval.metrics import auroc_binary, sm_ece_binary, snr_sigma_separation

__all__ = [
    "EVAL_MODELS",
    "HALUEVAL_KNOWN_FAILURE_ROW",
    "MultiModelEval",
    "load_eval_tuples",
    "format_r_ml_markdown_table",
    "compute_metrics_from_result_rows",
    "run_one_model_checkpointed",
]

# name -> (HF / API model id, active params label, architecture)
EVAL_MODELS: Dict[str, Tuple[str, str, str]] = {
    "gemma3-1b": ("google/gemma-3-1b-it", "1B", "dense"),
    "gemma3-4b": ("google/gemma-3-4b-it", "4B", "dense"),
    "gemma3-27b": ("google/gemma-3-27b-it", "27B", "dense"),
    "gemma4-e2b": ("google/gemma-4-e2b-it", "2.3B", "dense"),
    "gemma4-31b": ("google/gemma-4-31b-it", "31B", "dense"),
    "gemma4-26b-a4b": ("google/gemma-4-26b-a4b-it", "3.8B", "MoE"),
    "qwen3.6-a3b": ("Qwen/Qwen3.6-35B-A3B", "3B", "MoE"),
    "qwen3.6-27b": ("Qwen/Qwen3.6-27B", "27B", "dense"),
}

Pair = Tuple[str, str]  # prompt, gold

HALUEVAL_KNOWN_FAILURE_ROW: Dict[str, Any] = {
    "model": "ALL",
    "auroc": HALUEVAL_EXAMPLE_AUROC,
    "abstention_rate": None,
    "smece": None,
    "snr": None,
    "benchmark": "HaluEval QA",
    "status": "✗ fail",
    "note": "Published-style negative; archive probe metrics per CLAIM_DISCIPLINE.",
}


def load_eval_tuples(benchmark: str, *, data_path: Optional[Path] = None) -> List[Pair]:
    """Load ``(prompt, gold)`` pairs for ``truthfulqa`` | ``simpleqa`` | ``halueval``."""
    key = str(benchmark).strip().lower().replace("-", "_")
    if key == "truthfulqa":
        from cos.eval.truthfulqa import load_truthfulqa_rows

        rows = load_truthfulqa_rows(data_path)
        return [(str(r["prompt"]), str(r.get("expected", ""))) for r in rows]
    if key == "simpleqa":
        from cos.eval.simpleqa import load_simpleqa

        rows = load_simpleqa(data_path)
        return [(str(r["question"]), str(r.get("answer_gold", ""))) for r in rows]
    if key in ("halueval", "halu_eval"):
        from cos.eval.halueval import load_halueval_rows

        rows = load_halueval_rows(data_path)
        return [(str(r["prompt"]), str(r.get("expected", ""))) for r in rows]
    raise ValueError(f"unknown benchmark {benchmark!r} (try truthfulqa, simpleqa, halueval)")


def _saturation_status(benchmark: str) -> str:
    k = benchmark.lower()
    if "truthful" in k:
        return "⚠ saturated"
    if "halu" in k:
        return "✗ fail"
    return "✓ current"


class MultiModelEval:
    """Evaluate σ-gate on completions from an OpenAI-compatible HTTP backend."""

    def __init__(
        self,
        gate: Any,
        endpoint: str = "http://127.0.0.1:8000/v1",
        *,
        timeout_s: float = 120.0,
        generate_fn: Optional[Callable[[str, str], str]] = None,
    ) -> None:
        self.gate = gate
        self.endpoint = str(endpoint).rstrip("/")
        self.timeout_s = float(timeout_s)
        self._generate_fn = generate_fn

    def resolve_api_model(self, model_name: str) -> str:
        if model_name not in EVAL_MODELS:
            raise KeyError(f"unknown eval model {model_name!r}; known: {', '.join(sorted(EVAL_MODELS))}")
        return EVAL_MODELS[model_name][0]

    def _generate(self, model_name: str, prompt: str) -> str:
        if self._generate_fn is not None:
            return self._generate_fn(model_name, prompt)
        api_model = self.resolve_api_model(model_name)
        url = f"{self.endpoint}/chat/completions"
        payload = json.dumps(
            {
                "model": api_model,
                "messages": [{"role": "user", "content": str(prompt)}],
                "max_tokens": 256,
                "temperature": 0.3,
            }
        ).encode("utf-8")
        req = urllib.request.Request(
            url,
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=self.timeout_s) as resp:
                body = json.loads(resp.read().decode("utf-8", errors="replace"))
        except urllib.error.URLError as e:
            raise RuntimeError(f"POST {url} failed: {e}") from e
        try:
            return str(body["choices"][0]["message"]["content"])
        except (KeyError, IndexError, TypeError) as e:
            body_repr = repr(body)
            raise RuntimeError(f"unexpected completion shape: {body_repr[:500]}") from e

    @staticmethod
    def _check_correct(response: str, gold: str) -> bool:
        g = str(gold).strip().lower()
        if not g:
            return True
        if g in ("hallucination", "aligned"):
            r = str(response).lower()
            if g == "hallucination":
                return any(x in r for x in ("no", "false", "not ", "incorrect", "wrong"))
            return any(x in r for x in ("yes", "true", "correct", "right"))
        return g in str(response).lower()

    @staticmethod
    def _compute_metrics(results: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        if not results:
            return {
                "auroc": 0.5,
                "abstention_rate": 0.0,
                "smece": 0.0,
                "snr": 0.0,
                "n": 0,
            }
        y_true: List[int] = []
        y_err: List[int] = []
        sigmas: List[float] = []
        probs: List[float] = []
        abst = 0
        for r in results:
            correct = bool(r.get("correct"))
            y_true.append(1 if correct else 0)
            y_err.append(0 if correct else 1)
            s = float(r.get("sigma", 0.5))
            sigmas.append(s)
            probs.append(max(0.0, min(1.0, 1.0 - s)))
            if str(r.get("verdict", "")).upper() == "ABSTAIN":
                abst += 1
        n = len(results)
        abstention_rate = abst / max(n, 1)
        scores_for_auc = [1.0 - s for s in sigmas]
        auroc = float(auroc_binary(y_true, scores_for_auc))
        smece = float(sm_ece_binary(y_true, probs))
        snr = float(snr_sigma_separation(y_err, sigmas))
        return {
            "auroc": round(auroc, 6),
            "abstention_rate": round(abstention_rate, 6),
            "smece": round(smece, 6),
            "snr": round(snr, 6),
            "n": n,
            "not_agi_achieved": True,
        }

    def eval_model(
        self,
        model_name: str,
        dataset: Sequence[Union[Pair, Mapping[str, Any]]],
        n: int = 30,
        *,
        benchmark_label: str = "truthfulqa",
    ) -> Dict[str, Any]:
        pairs = self._normalize_pairs(dataset)
        lim = min(int(n), len(pairs))
        results: List[Dict[str, Any]] = []
        for i in range(lim):
            prompt, gold = pairs[i]
            response = self._generate(model_name, prompt)
            sigma, verdict = self.gate.score(prompt, response)
            correct = self._check_correct(response, gold)
            results.append(
                {
                    "prompt": prompt[:2000],
                    "response": response[:4000],
                    "gold": gold[:500],
                    "sigma": float(sigma),
                    "verdict": str(verdict),
                    "correct": correct,
                    "model": model_name,
                    "benchmark": benchmark_label,
                }
            )
        metrics = MultiModelEval._compute_metrics(results)
        metrics.update({"model": model_name, "benchmark": benchmark_label, "rows": results})
        return metrics

    def eval_all(
        self,
        dataset: Sequence[Union[Pair, Mapping[str, Any]]],
        n: int = 30,
        *,
        models: Optional[Sequence[str]] = None,
        benchmark_label: str = "truthfulqa",
    ) -> Dict[str, Any]:
        keys = list(models) if models is not None else list(EVAL_MODELS.keys())
        table: Dict[str, Any] = {"not_agi_achieved": True, "models": {}}
        for name in keys:
            try:
                table["models"][name] = self.eval_model(name, dataset, n, benchmark_label=benchmark_label)
            except Exception as e:
                table["models"][name] = {"error": str(e), "model": name}
        return table

    @staticmethod
    def _normalize_pairs(dataset: Sequence[Union[Pair, Mapping[str, Any]]]) -> List[Pair]:
        out: List[Pair] = []
        for item in dataset:
            if isinstance(item, (list, tuple)) and len(item) >= 2:
                out.append((str(item[0]), str(item[1])))
            elif isinstance(item, Mapping):
                p = str(item.get("prompt") or item.get("question") or "")
                g = str(item.get("expected") or item.get("answer_gold") or item.get("gold") or "")
                out.append((p, g))
        return out

    @staticmethod
    def print_mtier_table(table: Mapping[str, Any], *, stream_print: Callable[..., None] = print) -> None:
        """Plain-text summary (machine-friendly); use :func:`format_r_ml_markdown_table` for Reddit."""
        header = f"{'Model':20s} | {'AUROC':>6s} | {'Abstain%':>8s} | {'smECE':>6s} | {'SNR':>5s}"
        stream_print(header)
        stream_print("-" * len(header))
        for model in sorted(table.keys()):
            metrics = table[model]
            if "error" in metrics:
                stream_print(f"{model:20s} | {'ERROR':>6s} | {str(metrics['error'])[:38]}")
            else:
                ar = float(metrics.get("abstention_rate", 0.0))
                stream_print(
                    f"{model:20s} | {metrics.get('auroc', 0.0):6.3f} | "
                    f"{ar * 100:7.1f}% | {metrics.get('smece', 0.0):6.3f} | {metrics.get('snr', 0.0):5.1f}"
                )


def run_one_model_checkpointed(
    gate: Any,
    *,
    model_key: str,
    benchmark: str,
    n: int,
    endpoint: str,
    output_dir: str | Path,
    checkpoint_every: int = 5,
    run_id: Optional[str] = None,
    data_path: Optional[Path] = None,
    generate_fn: Optional[Callable[[str, str], str]] = None,
    progress: Optional[Callable[[int, int, Mapping[str, Any]], None]] = None,
) -> Dict[str, Any]:
    """One model × one benchmark with JSONL checkpoint every ``checkpoint_every`` rows."""
    import hashlib
    import time

    pairs = load_eval_tuples(benchmark, data_path=data_path)
    base = run_id or hashlib.sha256(str(time.time()).encode()).hexdigest()[:8]
    safe_model = re.sub(r"[^a-zA-Z0-9._-]+", "_", model_key)
    rid = f"{base}_{safe_model}_{str(benchmark).replace(',', '_')[:24]}"
    me = MultiModelEval(gate, endpoint, generate_fn=generate_fn)

    def eval_fn(i: int, pair: Pair) -> Mapping[str, Any]:
        prompt, gold = pair
        response = me._generate(model_key, prompt)
        sigma, verdict = gate.score(prompt, response)
        correct = MultiModelEval._check_correct(response, gold)
        return {
            "prompt": prompt[:2000],
            "response": response[:4000],
            "gold": gold[:500],
            "sigma": float(sigma),
            "verdict": str(verdict),
            "correct": bool(correct),
            "model": model_key,
            "benchmark": benchmark,
            "index": i,
        }

    meta = {
        "model_key": model_key,
        "benchmark": benchmark,
        "endpoint": endpoint,
        "n": int(n),
        "checkpoint_every": int(checkpoint_every),
        "data_path": str(data_path) if data_path else None,
    }
    ck = CheckpointEval(output_dir)
    rows = ck.run_with_checkpoint(
        eval_fn,
        pairs,
        int(n),
        checkpoint_every=checkpoint_every,
        run_id=rid,
        meta=meta,
        progress=progress,
    )
    metrics = compute_metrics_from_result_rows(rows)
    metrics.update(
        {
            "model": model_key,
            "benchmark": benchmark,
            "rows": rows,
            "status": _saturation_status(benchmark),
            "checkpoint_file": str(ck.checkpoint_path(rid)),
        }
    )
    return metrics


def format_r_ml_markdown_table(
    by_model_benchmark: Sequence[Mapping[str, Any]],
    *,
    include_known_halu: bool = True,
) -> str:
    """
    r/MachineLearning-oriented markdown: one row per (model, benchmark) + optional HaluEval fail row.
    """
    lines = [
        "| Model | AUROC | Abstain% | smECE | SNR | Benchmark | Status |",
        "|-------|-------|----------|-------|-----|-----------|--------|",
    ]

    def _cell_pct(x: Any) -> str:
        if x is None or (isinstance(x, float) and math.isnan(x)):
            return "—"
        try:
            return f"{float(x) * 100:.1f}%"
        except (TypeError, ValueError):
            return "—"

    def _cell_f(x: Any, nd: int = 3) -> str:
        if x is None:
            return "0.XXX"
        try:
            return f"{float(x):.{nd}f}"
        except (TypeError, ValueError):
            return "0.XXX"

    for row in by_model_benchmark:
        if "error" in row:
            lines.append(
                f"| {row.get('model', '—')} | ERROR | — | — | — | {row.get('benchmark', '—')} | — |"
            )
            continue
        lines.append(
            "| {model} | {auroc} | {abst} | {smece} | {snr} | {bench} | {st} |".format(
                model=str(row.get("model", "—")),
                auroc=_cell_f(row.get("auroc")),
                abst=_cell_pct(row.get("abstention_rate")),
                smece=_cell_f(row.get("smece")),
                snr=_cell_f(row.get("snr"), 1),
                bench=str(row.get("benchmark", "—")),
                st=str(row.get("status", "—")),
            )
        )

    if include_known_halu:
        lines.extend(
            [
                "",
                "**Known failures (always disclose):**",
                "",
                "| Model | AUROC | Abstain% | smECE | SNR | Benchmark | Status |",
                "|-------|-------|----------|-------|-----|-----------|--------|",
                "| {model} | {auroc:.3f} | — | — | — | {bench} | {st} |".format(
                    model=HALUEVAL_KNOWN_FAILURE_ROW["model"],
                    auroc=float(HALUEVAL_KNOWN_FAILURE_ROW["auroc"]),
                    bench=str(HALUEVAL_KNOWN_FAILURE_ROW["benchmark"]),
                    st=str(HALUEVAL_KNOWN_FAILURE_ROW["status"]),
                ),
                "",
                "_NOT AGI ACHIEVED. σ-gate is a detector; negatives belong in the Evidence Ladder._",
            ]
        )
    return "\n".join(lines)


def compute_metrics_from_result_rows(rows: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
    """Metric bundle for checkpoint-finalized rows (AUROC, abstention_rate, smECE, SNR)."""
    return MultiModelEval._compute_metrics(list(rows))


