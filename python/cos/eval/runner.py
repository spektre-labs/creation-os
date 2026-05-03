# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Glue eval loaders → :class:`~cos.bench.SigmaBench` rows."""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional

from cos.bench import SigmaBench
from cos.eval.halueval import load_halueval_rows
from cos.eval.hellaswag import load_hellaswag_rows
from cos.eval.mmlu import load_mmlu_rows
from cos.eval.triviaqa import load_triviaqa_rows
from cos.eval.truthfulqa import load_truthfulqa_rows
from cos.sigma_gate import SigmaGate

__all__ = ["run_benchmark_row_stub", "run_all_benchmarks_lab"]


def run_benchmark_row_stub(
    dataset: str,
    model: Callable[..., str],
    *,
    gate: Any = None,
    samples: Optional[List[Dict[str, str]]] = None,
    behavioral: bool = True,
) -> Dict[str, Any]:
    """Score ``dataset`` using stub rows + behavioral M-tier fields."""
    g = gate or SigmaGate()
    bench = SigmaBench()
    rows = samples
    if rows is None:
        d = str(dataset).lower()
        if d == "halu" or d == "halueval":
            rows = load_halueval_rows()
        elif d == "truthfulqa":
            rows = load_truthfulqa_rows()
        elif d == "triviaqa":
            rows = load_triviaqa_rows()
        elif d == "mmlu":
            rows = load_mmlu_rows()
        elif d == "hellaswag":
            rows = load_hellaswag_rows()
    name = str(dataset)
    return bench.run(name, g, model, samples=rows, behavioral=behavioral, probe_debug=True)


def run_all_benchmarks_lab(
    model: Callable[..., str],
    *,
    gate: Any = None,
) -> Dict[str, Any]:
    """Run demo rows for each dataset name (no downloads)."""
    keys = ("TruthfulQA", "TriviaQA", "HaluEval", "MMLU", "HellaSwag")
    out: Dict[str, Any] = {}
    for k in keys:
        out[k] = run_benchmark_row_stub(k, model, gate=gate, behavioral=True)
    return {"datasets": out, "disclaimer": "Demo rows only until JSONL paths are supplied."}
