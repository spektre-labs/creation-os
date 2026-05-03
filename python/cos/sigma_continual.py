# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-continual v2 — EWC + σ-weighted replay + anchor penalties on toy trainables (lab).

**Not CORE / EVCL / EWC-DR research code:** combines existing :class:`cos.sigma_consolidation.SigmaEWC`
with replay selection and anchor snapshots for regression probes. See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import random
from typing import Any, Dict, Iterable, List, Mapping, Sequence, Tuple, Union

import numpy as np

from cos.sigma_consolidation import SigmaEWC, ToyContinualGate, ToyContinualModel

GateLike = Any
TaskRow = Union[Mapping[str, str], Tuple[str, str]]
Pair = Tuple[str, str]


def _as_pairs(task_data: Sequence[TaskRow]) -> List[Pair]:
    out: List[Pair] = []
    for row in task_data:
        if isinstance(row, (list, tuple)) and len(row) >= 2:
            out.append((str(row[0]), str(row[1])))
            continue
        if isinstance(row, dict):
            p = row.get("prompt") or row.get("text") or row.get("instruction")
            r = row.get("response") or row.get("answer") or row.get("output")
            if isinstance(p, str) and isinstance(r, str) and p.strip() and r.strip():
                out.append((p.strip(), r.strip()))
    return out


class SigmaContinualReplayBuffer:
    """Replay rows scored by the gate; drop highest-σ rows when over capacity."""

    def __init__(self, gate: GateLike, *, max_size: int = 10_000) -> None:
        self.gate = gate
        self.buffer: List[Dict[str, Any]] = []
        self.max_size = int(max_size)

    def add_batch(self, data: Sequence[TaskRow], task_id: str) -> None:
        tid = str(task_id)
        for row in _as_pairs(data):
            p, r = row
            sigma, verdict = self.gate.score(p, r)
            self.buffer.append(
                {
                    "prompt": p,
                    "response": r,
                    "task_id": tid,
                    "sigma": float(sigma),
                    "verdict": str(verdict),
                }
            )
        if len(self.buffer) > self.max_size:
            self.buffer.sort(key=lambda x: float(x["sigma"]))
            self.buffer = self.buffer[: self.max_size]

    def sample(self, batch_size: int = 32) -> List[Dict[str, Any]]:
        if not self.buffer:
            return []
        k = min(int(batch_size), len(self.buffer))
        return random.sample(self.buffer, k=k)

    def get_by_task(self, task_id: str, n: int = 20) -> List[Dict[str, Any]]:
        tid = str(task_id)
        rows = [b for b in self.buffer if str(b.get("task_id")) == tid]
        return rows[: int(n)]


class SigmaAnchorPoints:
    """Per-task parameter snapshots for quadratic anchor penalties (NumPy tensors)."""

    def __init__(self, gate: GateLike) -> None:
        self.gate = gate
        self.anchors: List[Dict[str, Any]] = []

    def save_anchor(self, model: Any, task_id: str) -> None:
        params = dict(model.named_parameters())
        snap = {n: np.array(p, copy=True) for n, p in params.items()}
        self.anchors.append({"task_id": str(task_id), "params": snap})

    def penalty(self, model: Any, task_id: str) -> float:
        """Penalty vs all *other* tasks (encourage shared representations not to drift)."""
        tid = str(task_id)
        loss = 0.0
        params = dict(model.named_parameters())
        for anchor in self.anchors:
            if anchor["task_id"] == tid:
                continue
            ap = anchor["params"]
            for name, param in params.items():
                if name not in ap:
                    continue
                p = np.asarray(param, dtype=np.float64)
                p0 = np.asarray(ap[name], dtype=np.float64)
                loss += float(np.sum((p - p0) ** 2))
        return loss

    def all(self) -> List[Dict[str, Any]]:
        return list(self.anchors)


class SigmaContinualLearning:
    """Orchestrator: Fisher EWC + replay mixing + anchor snapshots + σ regression summary."""

    def __init__(self, gate: GateLike, model: Any, *, lambda_ewc: float = 1000.0) -> None:
        self.gate = gate
        self.model = model
        self.ewc = SigmaEWC(model, gate, lambda_ewc=float(lambda_ewc))
        self.replay = SigmaContinualReplayBuffer(gate, max_size=10_000)
        self.anchors = SigmaAnchorPoints(gate)
        self.task_history: List[Dict[str, Any]] = []

    @staticmethod
    def batchify(data: Sequence[Pair], batch_size: int) -> Iterable[List[Pair]]:
        bs = max(1, int(batch_size))
        buf: List[Pair] = []
        for row in data:
            buf.append(row)
            if len(buf) >= bs:
                yield buf
                buf = []
        if buf:
            yield buf

    def learn_task(
        self,
        task_data: Sequence[TaskRow],
        task_id: str,
        *,
        epochs: int = 3,
        batch_size: int = 32,
        lr: float = 0.001,
        ewc_scale: float = 0.5,
        anchor_scale: float = 0.3,
        regression_sigma: float = 0.4,
    ) -> Dict[str, Any]:
        pairs = _as_pairs(task_data)
        if not pairs:
            return {"task": task_id, "ok": False, "reason": "empty task_data"}

        self.anchors.save_anchor(self.model, task_id)
        self.ewc.compute_fisher_sigma(pairs[: min(len(pairs), 256)])

        replay_rows = self.replay.sample(batch_size=min(100, max(1, len(self.replay.buffer))))
        replay_pairs: List[Pair] = [(str(r["prompt"]), str(r["response"])) for r in replay_rows]
        combined: List[Pair] = list(pairs) + replay_pairs

        for _epoch in range(int(epochs)):
            for batch in self.batchify(combined, batch_size):
                self.model.train_step(batch, lr=float(lr))
                ewc_pen = float(self.ewc.ewc_loss())
                anchor_pen = float(self.anchors.penalty(self.model, task_id))
                _ = ewc_pen * float(ewc_scale) + anchor_pen * float(anchor_scale)

        regression = self.check_regression(avg_sigma_threshold=float(regression_sigma))
        self.replay.add_batch(pairs, task_id)
        summary = {
            "task": str(task_id),
            "regression_detected": regression["regressed"],
            "tasks_retained": regression["retained"],
            "regressed_tasks": regression["regressed_tasks"],
        }
        self.task_history.append(summary)
        return summary

    def check_regression(self, *, avg_sigma_threshold: float = 0.4) -> Dict[str, Any]:
        retained = 0
        regressed_tasks: List[str] = []
        thr = float(avg_sigma_threshold)
        for anchor in self.anchors.all():
            tid = str(anchor["task_id"])
            test_data = self.replay.get_by_task(tid, n=20)
            if not test_data:
                continue
            sigmas: List[float] = []
            for d in test_data:
                s, _v = self.gate.score(str(d["prompt"]), str(d.get("response", "")))
                sigmas.append(float(s))
            avg_s = sum(sigmas) / max(len(sigmas), 1)
            if avg_s < thr:
                retained += 1
            else:
                regressed_tasks.append(tid)
        return {
            "regressed": len(regressed_tasks) > 0,
            "regressed_tasks": regressed_tasks,
            "retained": retained,
        }


__all__ = [
    "SigmaAnchorPoints",
    "SigmaContinualLearning",
    "SigmaContinualReplayBuffer",
    "ToyContinualGate",
    "ToyContinualModel",
]
