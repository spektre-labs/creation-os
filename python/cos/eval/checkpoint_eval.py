# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Checkpoint-resilient eval loops (resume after crash / kill).

Writes JSONL incrementally plus a sidecar ``.meta.json`` for ``cos bench --resume``."""
from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Mapping, MutableMapping, Optional, Sequence, TypeVar

__all__ = ["CheckpointEval", "load_checkpoint_meta", "iter_checkpoint_results"]

T = TypeVar("T")


class CheckpointEval:
    """Append-only checkpointing: one JSON object per line (no secrets in files)."""

    def __init__(self, output_dir: str | Path = "eval_results") -> None:
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def meta_path(self, run_id: str) -> Path:
        return self.output_dir / f"checkpoint_{run_id}.meta.json"

    def checkpoint_path(self, run_id: str) -> Path:
        return self.output_dir / f"checkpoint_{run_id}.jsonl"

    def results_path(self, run_id: str) -> Path:
        return self.output_dir / f"results_{run_id}.json"

    def run_with_checkpoint(
        self,
        eval_fn: Callable[[int, T], Mapping[str, Any]],
        dataset: Sequence[T],
        n: int,
        *,
        checkpoint_every: int = 5,
        run_id: Optional[str] = None,
        meta: Optional[MutableMapping[str, Any]] = None,
        progress: Optional[Callable[[int, int, Mapping[str, Any]], None]] = None,
    ) -> List[Dict[str, Any]]:
        """
        Call ``eval_fn(i, dataset[i])`` for ``i`` in ``[0, min(n,len(dataset)))``, skipping rows
        already present in the checkpoint JSONL.
        """
        rid = run_id or hashlib.sha256(str(time.time()).encode()).hexdigest()[:8]
        ckpt_path = self.checkpoint_path(rid)
        results_path = self.results_path(rid)
        meta_path = self.meta_path(rid)
        lim = min(int(n), len(dataset))
        meta_blob = dict(meta or {})
        meta_blob.update(
            {
                "run_id": rid,
                "n": lim,
                "checkpoint_every": int(checkpoint_every),
                "dataset_len": len(dataset),
                "output_dir": str(self.output_dir.resolve()),
                "updated_at": time.time(),
            },
        )
        meta_path.write_text(json.dumps(meta_blob, indent=2, default=str), encoding="utf-8")

        existing: List[Dict[str, Any]] = []
        if ckpt_path.is_file():
            for line in ckpt_path.read_text(encoding="utf-8").splitlines():
                if not line.strip():
                    continue
                existing.append(json.loads(line))
        done = len(existing)
        results: List[Dict[str, Any]] = list(existing)

        for i in range(done, lim):
            row = dict(eval_fn(i, dataset[i]))
            results.append(row)
            with ckpt_path.open("a", encoding="utf-8") as f:
                f.write(json.dumps(row, default=str) + "\n")
            if progress and ((i + 1) % int(checkpoint_every) == 0 or i + 1 == lim):
                progress(i + 1, lim, row)

        results_path.write_text(json.dumps(results, indent=2, default=str), encoding="utf-8")
        return results

    def continue_from_path(
        self,
        checkpoint_jsonl: Path | str,
        eval_fn: Callable[[int, T], Mapping[str, Any]],
        dataset: Sequence[T],
        n: int,
        *,
        checkpoint_every: int = 5,
        progress: Optional[Callable[[int, int, Mapping[str, Any]], None]] = None,
    ) -> List[Dict[str, Any]]:
        """Append to an existing JSONL checkpoint; refresh ``results_*.json`` when complete."""
        ckpt_path = Path(checkpoint_jsonl).resolve()
        results = iter_checkpoint_results(ckpt_path)
        done = len(results)
        lim = min(int(n), len(dataset))
        stem = ckpt_path.stem
        rid = stem[len("checkpoint_") :] if stem.startswith("checkpoint_") else stem
        results_path = ckpt_path.parent / f"results_{rid}.json"
        for i in range(done, lim):
            row = dict(eval_fn(i, dataset[i]))
            results.append(row)
            with ckpt_path.open("a", encoding="utf-8") as f:
                f.write(json.dumps(row, default=str) + "\n")
            if progress and ((i + 1) % int(checkpoint_every) == 0 or i + 1 == lim):
                progress(i + 1, lim, row)
        meta_path = ckpt_path.with_name(stem + ".meta.json")
        if meta_path.is_file():
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
            meta["updated_at"] = time.time()
            meta["resumed_from"] = done
            meta_path.write_text(json.dumps(meta, indent=2, default=str), encoding="utf-8")
        results_path.write_text(json.dumps(results, indent=2, default=str), encoding="utf-8")
        return results


def load_checkpoint_meta(checkpoint_jsonl: Path | str) -> Dict[str, Any]:
    """Load sidecar meta for a checkpoint stem (``checkpoint_xyz.jsonl`` → ``checkpoint_xyz.meta.json``)."""
    p = Path(checkpoint_jsonl)
    meta = p.with_name(p.stem + ".meta.json")
    if not meta.is_file():
        return {}
    return json.loads(meta.read_text(encoding="utf-8"))


def iter_checkpoint_results(checkpoint_jsonl: Path | str) -> List[Dict[str, Any]]:
    p = Path(checkpoint_jsonl)
    if not p.is_file():
        return []
    out: List[Dict[str, Any]] = []
    for line in p.read_text(encoding="utf-8").splitlines():
        if line.strip():
            out.append(json.loads(line))
    return out
