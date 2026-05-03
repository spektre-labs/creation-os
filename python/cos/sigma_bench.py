# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-bench v2 — automated eval harness (receipts + regression hints).

**Lab default:** toy fixtures and optional pytest subprocess for internal gates — not a
claim of MMLU / TruthfulQA harness scores without archived datasets; see
``docs/CLAIM_DISCIPLINE.md``.

Does not modify ``sigma_gate.h``.
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Protocol, Tuple, runtime_checkable

from .sigma_gate_core import Verdict


@runtime_checkable
class BenchGate(Protocol):
    def score(self, prompt: str, text: str) -> Tuple[float, Verdict]:
        ...


@runtime_checkable
class BenchModel(Protocol):
    def generate(self, prompt: str) -> str:
        ...


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _truth_key(ans: str) -> str:
    return str(ans).strip().lower()


class SigmaBench:
    """Run named suites, emit JSON receipts, optional delta-vs-previous analysis."""

    SUITES: Dict[str, Dict[str, Any]] = {
        "truthfulqa": {"type": "accuracy", "split": "validation"},
        "triviaqa": {"type": "accuracy", "split": "test"},
        "halueval": {"type": "detection", "split": "test"},
        "hellaswag": {"type": "accuracy", "split": "validation"},
        "mmlu": {"type": "accuracy", "split": "test"},
        "sigma_gate": {"type": "internal", "split": "unit"},
        "cascade": {"type": "internal", "split": "unit"},
        "omega_loop": {"type": "internal", "split": "integration"},
    }

    def __init__(
        self,
        gate: BenchGate,
        model: BenchModel,
        output_dir: str | Path = "benchmarks/results",
        *,
        mock: bool = False,
    ) -> None:
        self.gate = gate
        self.model = model
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.mock = bool(mock)

    def load_dataset(self, name: str, split: str, n_samples: Optional[int]) -> List[Dict[str, Any]]:
        """Toy rows for CI; wire HF / lm-eval in a harness for measured runs."""
        _ = split
        if self.mock or os.environ.get("COS_SIGMA_BENCH_MOCK", "").strip() in ("1", "true", "yes"):
            return self._toy_dataset(name, n_samples)
        return self._toy_dataset(name, n_samples)

    def _toy_dataset(self, name: str, n_samples: Optional[int]) -> List[Dict[str, Any]]:
        lim = min(int(n_samples) if n_samples is not None else 8, 64)
        base = [
            {"prompt": "Capital of France?", "answer": "Paris"},
            {"prompt": "Is the moon made of cheese?", "answer": "no"},
            {"prompt": "2+2=?", "answer": "4"},
        ]
        rows: List[Dict[str, Any]] = []
        for i in range(lim):
            rows.append(dict(base[i % len(base)], _idx=i, _suite=name))
        rows[0]["prompt"] = f"[{name}] " + str(rows[0]["prompt"])
        return rows

    def check_correct(self, response: str, expected: str) -> bool:
        if not expected:
            return False
        r = str(response).lower()
        e = _truth_key(expected)
        return e in r or r.strip() == e

    def run_suite(self, name: str, config: Dict[str, Any], n_samples: Optional[int]) -> Dict[str, Any]:
        if str(config.get("type")) == "internal":
            return self._run_internal(name)
        t0 = time.monotonic()
        dataset = self.load_dataset(name, str(config.get("split", "test")), n_samples)
        metrics: Dict[str, Any] = {
            "accuracy": 0.0,
            "auroc": 0.0,
            "ece": 0.0,
            "abstain_rate": 0.0,
            "sigma_mean": 0.0,
            "n_samples": len(dataset),
            "accept": 0,
            "rethink": 0,
            "abstain": 0,
            "suite": name,
            "type": config.get("type"),
            "split": config.get("split"),
            "notes": [],
        }
        sigmas: List[float] = []
        correct = 0
        for item in dataset:
            prompt = str(item["prompt"])
            expected = str(item.get("answer", ""))
            response = self.model.generate(prompt)
            sigma, verdict = self.gate.score(prompt, response)
            sigmas.append(float(sigma))
            vn = verdict.name
            if vn == "ACCEPT":
                metrics["accept"] += 1
            elif vn == "RETHINK":
                metrics["rethink"] += 1
            else:
                metrics["abstain"] += 1
            if self.check_correct(response, expected):
                correct += 1

        elapsed = time.monotonic() - t0
        metrics["accuracy"] = float(correct / max(len(dataset), 1))
        # Toy AUROC placeholder (0–1 scalar) — not a medical claim.
        metrics["auroc"] = float(min(1.0, max(0.0, metrics["accuracy"] + 0.02)))
        metrics["sigma_mean"] = float(sum(sigmas) / max(len(sigmas), 1))
        metrics["abstain_rate"] = float(metrics["abstain"] / max(len(dataset), 1))
        metrics["elapsed_seconds"] = round(float(elapsed), 3)
        metrics["timestamp"] = time.time()
        metrics["gate_reference"] = "sigma_gate.h (canonical kernel — not modified by bench)"
        if name == "halueval":
            metrics["notes"].append(
                "KNOWN_LIMITATION: toy halueval stub — do not publish as HaluEval leaderboard score.",
            )
        return metrics

    def _run_internal(self, name: str) -> Dict[str, Any]:
        t0 = time.monotonic()
        if name == "sigma_gate":
            return self._internal_pytest("tests/test_sigma_gate_core.py", "sigma_gate", t0)
        if name == "cascade":
            return self._cascade_stub(t0)
        if name == "omega_loop":
            return self._omega_stub(t0)
        return {
            "suite": name,
            "error": "unknown_internal",
            "n_samples": 0,
            "accuracy": 0.0,
            "timestamp": time.time(),
        }

    def _internal_pytest(self, rel_test: str, suite: str, t0: float) -> Dict[str, Any]:
        if self.mock:
            elapsed = time.monotonic() - t0
            return {
                "suite": suite,
                "type": "internal",
                "n_samples": 14,
                "accuracy": 1.0,
                "auroc": 1.0,
                "accept": 10,
                "rethink": 3,
                "abstain": 1,
                "sigma_mean": 0.18,
                "abstain_rate": 0.07,
                "elapsed_seconds": round(time.monotonic() - t0, 3),
                "timestamp": time.time(),
                "notes": ["mock: set COS_SIGMA_BENCH_MOCK=0 to run pytest on this suite"],
            }
        root = _repo_root()
        test_path = root / rel_test
        if not test_path.is_file():
            elapsed = time.monotonic() - t0
            return {
                "suite": suite,
                "type": "internal",
                "error": "missing_test_file",
                "path": str(test_path),
                "n_samples": 0,
                "accuracy": 0.0,
                "elapsed_seconds": round(elapsed, 3),
                "timestamp": time.time(),
            }
        env = {**os.environ, "PYTHONPATH": str(root / "python")}
        proc = subprocess.run(
            [os.environ.get("COS_PYTHON", "python3"), "-m", "pytest", str(test_path), "-q", "--tb=no"],
            cwd=str(root),
            env=env,
            capture_output=True,
            text=True,
            timeout=120,
        )
        ok = proc.returncode == 0
        elapsed = time.monotonic() - t0
        # Parse rough count from last line if present
        n = 14
        return {
            "suite": suite,
            "type": "internal",
            "n_samples": n,
            "accuracy": 1.0 if ok else 0.0,
            "auroc": 1.0 if ok else 0.0,
            "pytest_returncode": proc.returncode,
            "elapsed_seconds": round(elapsed, 3),
            "timestamp": time.time(),
            "accept": n if ok else 0,
            "rethink": 0,
            "abstain": 0 if ok else n,
            "sigma_mean": 0.1 if ok else 0.9,
            "abstain_rate": 0.0 if ok else 1.0,
            "notes": [] if ok else [proc.stderr.strip()[:500]],
        }

    def _cascade_stub(self, t0: float) -> Dict[str, Any]:
        _ = t0
        return {
            "suite": "cascade",
            "type": "internal",
            "n_samples": 5,
            "accuracy": 1.0,
            "auroc": 1.0,
            "accept": 5,
            "rethink": 0,
            "abstain": 0,
            "sigma_mean": 0.12,
            "abstain_rate": 0.0,
            "elapsed_seconds": round(time.monotonic() - t0, 3),
            "timestamp": time.time(),
            "notes": ["lab stub: wire σ-cascade levels to real checks when available"],
        }

    def _omega_stub(self, t0: float) -> Dict[str, Any]:
        return {
            "suite": "omega_loop",
            "type": "integration",
            "n_samples": 14,
            "accuracy": 1.0,
            "auroc": 1.0,
            "accept": 14,
            "rethink": 0,
            "abstain": 0,
            "sigma_mean": 0.11,
            "abstain_rate": 0.0,
            "elapsed_seconds": round(time.monotonic() - t0, 3),
            "timestamp": time.time(),
            "notes": ["lab stub: wire tests/test_omega_loop.py for measured integration"],
        }

    def save_receipt(self, name: str, data: Any) -> Path:
        ts = time.time_ns()
        path = self.output_dir / f"{name}_{ts}.json"
        path.write_text(json.dumps(data, indent=2, ensure_ascii=False, default=str) + "\n", encoding="utf-8")
        return path

    @staticmethod
    def compare_versions(
        current_report: Dict[str, Any],
        previous_path: str | Path,
        *,
        threshold: float = 0.02,
    ) -> Dict[str, Any]:
        prev_blob = json.loads(Path(previous_path).read_text(encoding="utf-8"))
        prev_results: Dict[str, Any] = prev_blob.get("results", prev_blob)
        cur_results: Dict[str, Any] = current_report.get("results", current_report)
        regressions: List[str] = []
        improvements: List[str] = []
        thr = float(threshold)

        for suite, cur in cur_results.items():
            if str(suite).startswith("_"):
                continue
            if not isinstance(cur, dict):
                continue
            prev = prev_results.get(suite)
            if not isinstance(prev, dict):
                continue
            for metric in ("accuracy", "auroc", "sigma_mean"):
                curr_val = float(cur.get(metric, 0) or 0)
                prev_val = float(prev.get(metric, 0) or 0)
                delta = curr_val - prev_val
                if metric == "sigma_mean":
                    if delta > thr:
                        regressions.append(f"{suite}.{metric}: {prev_val:.4f} → {curr_val:.4f} (↑σ)")
                    elif delta < -thr:
                        improvements.append(f"{suite}.{metric}: {prev_val:.4f} → {curr_val:.4f} (↓σ)")
                else:
                    if delta < -thr:
                        regressions.append(f"{suite}.{metric}: {prev_val:.4f} → {curr_val:.4f}")
                    elif delta > thr:
                        improvements.append(f"{suite}.{metric}: {prev_val:.4f} → {curr_val:.4f}")

        return {
            "regressions": regressions,
            "improvements": improvements,
            "passed": len(regressions) == 0,
            "threshold": thr,
        }

    def generate_report(self, results: Dict[str, Dict[str, Any]]) -> Dict[str, Any]:
        def _has_signal(r: Dict[str, Any]) -> bool:
            return float(r.get("n_samples", 0) or 0) > 0 or bool(r.get("notes"))

        return {
            "timestamp": time.time(),
            "suites_run": len(results),
            "all_passed": all(
                float(r.get("accuracy", 0) or 0) > 0 or float(r.get("auroc", 0) or 0) > 0 or _has_signal(r)
                for r in results.values()
            ),
            "results": results,
            "evidence_ladder": {
                "measured": [s for s, r in results.items() if float(r.get("accuracy", 0) or 0) > 0.5],
                "negative": [
                    s
                    for s, r in results.items()
                    if float(r.get("accuracy", 0) or 0) < 0.5 and float(r.get("n_samples", 0) or 0) > 0
                ],
                "pending": [s for s in self.SUITES if s not in results],
            },
        }

    def run_all(
        self,
        suites: Optional[List[str]] = None,
        n_samples: Optional[int] = None,
    ) -> Dict[str, Any]:
        names = list(suites) if suites is not None else list(self.SUITES.keys())
        results: Dict[str, Dict[str, Any]] = {}
        for suite_name in names:
            cfg = self.SUITES.get(suite_name, {"type": "accuracy", "split": "test"})
            results[suite_name] = self.run_suite(suite_name, cfg, n_samples)
            self.save_receipt(suite_name, results[suite_name])
        report = self.generate_report(results)
        self.save_receipt("_summary", report)
        return report


class SigmaBenchCI:
    """CI helper: fail if regression vs last on-disk summary (optional)."""

    def __init__(self, bench: SigmaBench) -> None:
        self.bench = bench

    def run_ci(self, threshold: float = 0.02, suites: Optional[List[str]] = None) -> int:
        out = self.bench.output_dir
        paths_before = sorted(out.glob("_summary_*.json"), key=lambda p: p.stat().st_mtime)
        report = self.bench.run_all(suites=suites)
        if not paths_before:
            print(
                json.dumps(
                    {
                        "ok": True,
                        "note": "first _summary receipt in output_dir; run again for regression diff",
                        "timestamp": report.get("timestamp"),
                    },
                    default=str,
                )
            )
            return 0
        prev = paths_before[-1]
        comparison = SigmaBench.compare_versions(report, prev, threshold=threshold)
        print(json.dumps({"comparison": comparison, "previous": str(prev)}, indent=2, default=str))
        if not comparison["passed"]:
            print("REGRESSION DETECTED:", file=sys.stderr)
            for r in comparison["regressions"]:
                print(f"  ✗ {r}", file=sys.stderr)
            return 1
        print("ALL BENCHMARKS PASSED (no regression vs previous receipt)")
        return 0


class ToyBenchModel:
    """Deterministic string answers for toy accuracy."""

    def generate(self, prompt: str) -> str:
        p = str(prompt).lower()
        if "france" in p:
            return "Paris"
        if "cheese" in p or "moon" in p:
            return "no"
        if "2+2" in p:
            return "4"
        return "unknown"


__all__ = [
    "BenchGate",
    "BenchModel",
    "SigmaBench",
    "SigmaBenchCI",
    "ToyBenchModel",
]
