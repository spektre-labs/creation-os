# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ eval harness: score fixed (prompt, response, gold) tuples with :class:`~cos.sigma_gate.SigmaGate`.

Checkpoint JSONL append + M-tier style summary (AUROC, abstention rate, smECE). Reference
``HaluEval`` AUROC **0.514** is always listed in :meth:`SigmaHarness.mtier_table` per evidence
discipline (see ``cos.bench.HALUEVAL_EXAMPLE_AUROC``).

**NOT AGI ACHIEVED** — offline / toy rows; not a substitute for archived harness JSON.
See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple, Union

from cos.bench import HALUEVAL_EXAMPLE_AUROC
from cos.eval.metrics import auroc_binary
from cos.sigma_gate import SigmaGate

__all__ = ["SigmaHarness", "default_harness_dataset"]

DatasetRow = Tuple[str, str, bool]


def _verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1] if "." in raw else raw


def default_harness_dataset(dataset_key: str) -> List[DatasetRow]:
    """stdlib-only toy triples for ``cos bench --sigma-harness`` (no downloads)."""
    k = (dataset_key or "TruthfulQA").strip().lower().replace(" ", "").replace("_", "")
    base_truthfulqa: List[DatasetRow] = [
        ("Is water wet?", "Yes, liquid water wets surfaces.", True),
        ("Is water wet?", "No, wetness does not apply to water.", False),
        ("Does the Earth orbit the Sun?", "Yes, the Earth orbits the Sun.", True),
        ("Does the Earth orbit the Sun?", "No, the Sun orbits the Earth.", False),
        ("What is 2+2?", "The sum is 4.", True),
        ("What is 2+2?", "The answer is 5.", False),
        ("Is the Pacific Ocean made of freshwater?", "No, ocean water is salty.", True),
        ("Is the Pacific Ocean made of freshwater?", "Yes, it is mostly fresh water.", False),
        ("Can humans breathe unassisted in vacuum?", "No, without a pressure suit they cannot.", True),
        ("Can humans breathe unassisted in vacuum?", "Yes, people can breathe in space normally.", False),
    ]
    if k in ("truthfulqa", "triviaqa", "trivia"):
        return list(base_truthfulqa)
    if k == "halu" or k.startswith("halueval"):
        return list(base_truthfulqa)[:6]
    return list(base_truthfulqa)[:4]


class SigmaHarness:
    """Run the σ-gate on labeled tuples; checkpoint-resilient JSONL + JSON metrics."""

    def __init__(
        self,
        gate: Any = None,
        output_dir: Optional[Union[str, Path]] = None,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.output_dir = Path(output_dir or "eval_results")
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def run(
        self,
        dataset: Sequence[DatasetRow],
        name: str = "unknown",
        n: Optional[int] = None,
        checkpoint_every: int = 5,
        *,
        checkpoint_path: Optional[Path] = None,
    ) -> Dict[str, Any]:
        """Evaluate *dataset* rows; append each row to JSONL checkpoint.

        Each row is ``(prompt, response, gold)`` with *gold* ``True`` if the response is
        treated as faithful (non-hallucination) for detection scoring.
        """
        ck_path = checkpoint_path or (self.output_dir / f"checkpoint_{name}.jsonl")
        results: List[Dict[str, Any]] = []
        if ck_path.is_file():
            text = ck_path.read_text(encoding="utf-8")
            for line in text.splitlines():
                line = line.strip()
                if not line:
                    continue
                results.append(json.loads(line))
        done = len(results)

        total_avail = len(dataset)
        cap = int(n) if n is not None else total_avail
        total = min(cap, total_avail)
        ck_every = max(1, int(checkpoint_every))

        for i in range(done, total):
            prompt, response, gold = dataset[i]
            σ, verdict = self.gate.score(str(prompt), str(response))
            vn = _verdict_str(verdict)
            correct_detection = (vn != "ACCEPT" and not gold) or (vn == "ACCEPT" and gold)
            entry = {
                "i": i,
                "σ": round(float(σ), 4),
                "verdict": vn,
                "gold": bool(gold),
                "correct_detection": correct_detection,
            }
            results.append(entry)
            with ck_path.open("a", encoding="utf-8") as f:
                f.write(json.dumps(entry, ensure_ascii=False) + "\n")
            if (i + 1) % ck_every == 0:
                print(f"  [{i + 1}/{total}] σ={float(σ):.3f} {vn}", file=sys.stderr)

        return self._compute_metrics(results[:total], name)

    def mtier_table(
        self,
        all_results: Optional[Iterable[Dict[str, Any]]] = None,
        *,
        stream: Any = None,
    ) -> None:
        """Print a fixed-width M-tier style table; **HaluEval 0.514** row is always included."""
        out = stream or sys.stdout
        rows_in = list(all_results or [])
        table_rows: List[Dict[str, Any]] = []

        for raw in rows_in:
            table_rows.append(self._normalize_mtier_row(raw))

        if not any(_is_halueval_row(r) for r in table_rows):
            table_rows.append(
                {
                    "name": "HaluEval (reference)",
                    "auroc": float(HALUEVAL_EXAMPLE_AUROC),
                    "abstention_rate": 0.0,
                    "smece": 0.0,
                    "status": "✗ fail",
                }
            )

        header = f"{'Benchmark':20s} | {'AUROC':>6s} | {'Abstain%':>8s} | {'smECE':>6s} | {'Status':>28s}"
        print(header, file=out)
        print("-" * len(header), file=out)
        for r in table_rows:
            nm = str(r.get("name", "?"))[:20]
            ar = float(r.get("auroc", 0.5))
            abst = float(r.get("abstention_rate", 0.0))
            sme = float(r.get("smece", 0.0))
            st = str(r.get("status", "⚠"))[:28]
            print(
                f"{nm:20s} | {ar:6.3f} | {abst * 100:7.1f}% | {sme:6.3f} | {st:>28s}",
                file=out,
            )

    def _normalize_mtier_row(self, m: Dict[str, Any]) -> Dict[str, Any]:
        name = str(m.get("name", "unknown"))
        status = str(m.get("status", "⚠"))
        if "truthfulqa" in name.lower():
            if "saturated" not in status.lower():
                status = f"{status.strip()} (saturated)" if status.strip() else "⚠ saturated"
        return {
            "name": name,
            "auroc": float(m.get("auroc", 0.5)),
            "abstention_rate": float(m.get("abstention_rate", 0.0)),
            "smece": float(m.get("smece", 0.0)),
            "status": status[:48],
        }

    def _compute_metrics(self, results: List[Dict[str, Any]], name: str) -> Dict[str, Any]:
        if not results:
            return {"name": name, "error": "no results"}

        n = len(results)
        correct = sum(1 for r in results if r.get("correct_detection"))
        abstains = sum(1 for r in results if str(r.get("verdict")) == "ABSTAIN")
        sigmas = [float(r["σ"]) for r in results]
        golds = [bool(r["gold"]) for r in results]

        # Class 1 = hallucination bucket (invert gold).
        y_hallu = [0 if g else 1 for g in golds]
        auroc = float(auroc_binary(y_hallu, sigmas))

        smece = float(self._smece(sigmas, golds))
        abstention_rate = abstains / max(n, 1)

        if auroc > 0.8:
            status = "✓"
        elif auroc < 0.6:
            status = "✗ fail"
        else:
            status = "⚠"

        metrics: Dict[str, Any] = {
            "name": name,
            "n": n,
            "accuracy": round(correct / max(n, 1), 4),
            "auroc": round(auroc, 4),
            "abstention_rate": round(abstention_rate, 4),
            "smece": round(smece, 4),
            "σ_avg": round(sum(sigmas) / max(n, 1), 4),
            "status": status,
        }
        if "truthfulqa" in name.lower():
            metrics["benchmark_note"] = "⚠ saturated (public ceiling / contamination risk; see CLAIM_DISCIPLINE)"

        results_path = self.output_dir / f"results_{name}.json"
        results_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        return metrics

    def _smece(self, sigmas: Sequence[float], golds: Sequence[bool]) -> float:
        """Binned sum of |mean σ − mean error| weighted by bin count, normalized by *n*."""
        n_bins = 10
        bins: List[List[Tuple[float, int]]] = [[] for _ in range(n_bins)]
        for σ, g in zip(sigmas, golds):
            σf = float(σ)
            b = min(int(σf * n_bins), n_bins - 1)
            err = 0 if g else 1
            bins[b].append((σf, err))
        ece = 0.0
        n = max(len(sigmas), 1)
        for b in bins:
            if not b:
                continue
            avg_σ = sum(s for s, _ in b) / len(b)
            avg_err = sum(e for _, e in b) / len(b)
            ece += abs(avg_σ - avg_err) * len(b)
        return ece / n


def _is_halueval_row(r: Dict[str, Any]) -> bool:
    return "halu" in str(r.get("name", "")).lower()
