# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""v139 σ-bench v2: harness receipts + regression compare (toy / mock)."""
from __future__ import annotations

import json
from pathlib import Path

from cos.sigma_bench import SigmaBench, SigmaBenchCI, ToyBenchModel
from cos.sigma_gate_core import Verdict
from cos.sigma_gate_quickscore import quickscore


class _Gate:
    def score(self, prompt: str, response: str):
        s, d = quickscore(prompt, response)
        return float(s), Verdict[str(d)]


def test_sigma_bench_three_suites_receipts_and_compare(tmp_path: Path) -> None:
    out = tmp_path / "results"
    suites = ["truthfulqa", "triviaqa", "mmlu"]
    bench = SigmaBench(_Gate(), ToyBenchModel(), out, mock=True)
    bench.run_all(suites=suites, n_samples=4)
    summaries = sorted(out.glob("_summary_*.json"), key=lambda p: p.stat().st_mtime)
    assert len(summaries) == 1
    receipt_names = {p.name.split("_")[0] for p in out.glob("*.json") if not p.name.startswith("_summary")}
    assert {"truthfulqa", "triviaqa", "mmlu"}.issubset(receipt_names)

    bench.run_all(suites=suites, n_samples=4)
    summaries = sorted(out.glob("_summary_*.json"), key=lambda p: p.stat().st_mtime)
    assert len(summaries) >= 2
    cur = json.loads(summaries[-1].read_text(encoding="utf-8"))
    comp = SigmaBench.compare_versions(cur, summaries[-2], threshold=0.02)
    assert comp["passed"], comp


def test_sigma_bench_ci_second_run_no_regression(tmp_path: Path) -> None:
    bench = SigmaBench(_Gate(), ToyBenchModel(), tmp_path, mock=True)
    assert SigmaBenchCI(bench).run_ci(suites=["truthfulqa", "mmlu"]) == 0
    assert SigmaBenchCI(bench).run_ci(suites=["truthfulqa", "mmlu"]) == 0
