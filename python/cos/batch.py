# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""Parallel σ-scoring for many (prompt, response) pairs via :class:`concurrent.futures.ProcessPoolExecutor`.

Stdlib only (no extra deps). Each worker constructs its own :class:`~cos.sigma_gate.SigmaGate`.
Throughput is **microbench-local**; do not merge with harness MMLU/ARC headlines
(see ``docs/CLAIM_DISCIPLINE.md``). **NOT AGI.**
"""
from __future__ import annotations

import json
import os
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple, Union

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaBatch", "score_one_pair"]

# Picklable top-level worker target (required for process pools).
Prompt = str
Response = str
Task = Tuple[Prompt, Response, int]


def score_one_pair(task: Task) -> Dict[str, Any]:
    """Score one pair in a worker process."""
    prompt, response, idx = task
    gate = SigmaGate()
    sigma, verdict = gate.score(prompt, response)
    return {
        "idx": idx,
        "prompt": prompt[:80],
        "response": response[:80],
        "sigma": round(float(sigma), 4),
        "verdict": str(verdict),
    }


class SigmaBatch:
    """Batch σ-scoring with a process pool (bypasses the CPython GIL per worker)."""

    def __init__(self, workers: Optional[int] = None) -> None:
        n = os.cpu_count() or 1
        self.workers = int(workers) if workers is not None else max(1, n - 1)

    def score_pairs(self, pairs: Sequence[Tuple[str, str]]) -> Dict[str, Any]:
        """Score ``pairs`` in parallel; return sorted rows plus aggregate counters."""
        tasks: List[Task] = [(p, r, i) for i, (p, r) in enumerate(pairs)]
        results: List[Dict[str, Any]] = []

        if not tasks:
            return {
                "results": [],
                "total": 0,
                "avg_sigma": 0.0,
                "accept": 0,
                "rethink": 0,
                "abstain": 0,
                "elapsed_s": 0.0,
                "throughput": 0.0,
                "workers": self.workers,
            }

        start = time.perf_counter()
        with ProcessPoolExecutor(max_workers=self.workers) as pool:
            future_map = {pool.submit(score_one_pair, t): t for t in tasks}
            for fut in as_completed(future_map):
                task = future_map[fut]
                try:
                    results.append(fut.result())
                except Exception as exc:  # noqa: BLE001
                    results.append(
                        {
                            "idx": task[2],
                            "prompt": task[0][:80],
                            "response": task[1][:80],
                            "sigma": 1.0,
                            "verdict": "ABSTAIN",
                            "error": str(exc),
                        }
                    )

        elapsed = time.perf_counter() - start
        results.sort(key=lambda r: int(r["idx"]))

        sigmas = [float(r["sigma"]) for r in results]
        return {
            "results": results,
            "total": len(results),
            "avg_sigma": round(sum(sigmas) / len(sigmas), 4),
            "accept": sum(1 for r in results if r.get("verdict") == "ACCEPT"),
            "rethink": sum(1 for r in results if r.get("verdict") == "RETHINK"),
            "abstain": sum(1 for r in results if r.get("verdict") == "ABSTAIN"),
            "elapsed_s": round(elapsed, 2),
            "throughput": round(len(results) / max(elapsed, 1e-9), 1),
            "workers": self.workers,
        }

    def score_jsonl(
        self,
        input_path: Union[str, Path],
        output_path: Optional[Union[str, Path]] = None,
        *,
        encoding: str = "utf-8",
    ) -> Dict[str, Any]:
        """Read JSONL with ``prompt`` and ``response`` per line; optionally write scored JSONL."""
        in_p = Path(input_path)
        pairs: List[Tuple[str, str]] = []
        with in_p.open("r", encoding=encoding) as f:
            for lineno, line in enumerate(f, start=1):
                raw = line.strip()
                if not raw:
                    continue
                try:
                    obj = json.loads(raw)
                except json.JSONDecodeError as exc:
                    raise ValueError(f"line {lineno}: invalid JSON ({exc})") from exc
                pairs.append((str(obj.get("prompt", "")), str(obj.get("response", ""))))

        summary = self.score_pairs(pairs)

        if output_path is not None:
            out_p = Path(output_path)
            out_p.parent.mkdir(parents=True, exist_ok=True)
            with out_p.open("w", encoding=encoding) as out:
                for row in summary["results"]:
                    out.write(json.dumps(row, ensure_ascii=False) + "\n")

        return summary
