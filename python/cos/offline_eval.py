# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-offline-eval — local JSONL eval loop with checkpoints (no network).

``model_path`` is recorded for provenance; this module does **not** load PyTorch."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import json
from pathlib import Path
from typing import Any, Dict, Iterator, List, Mapping, Optional

__all__ = ["SigmaOfflineEval"]


class SigmaOfflineEval:
    """Stream questions, write incremental results + periodic checkpoints."""

    def __init__(self) -> None:
        self._checkpoint_interval = 50

    @staticmethod
    def checkpoint_every(n_questions: int) -> int:
        return max(1, int(n_questions))

    @staticmethod
    def batch_size_auto(available_ram_gb: float) -> int:
        g = max(0.5, float(available_ram_gb))
        return max(1, min(64, int(g * 1.5)))

    @staticmethod
    def progress_bar(total: int, current: int, *, width: int = 30) -> str:
        total = max(1, int(total))
        cur = max(0, min(int(current), total))
        frac = cur / total
        filled = int(width * frac)
        bar = "#" * filled + "-" * (width - filled)
        return f"[{bar}] {cur}/{total} ({100.0 * frac:.1f}%)"

    @staticmethod
    def resume(checkpoint_path: str | Path) -> Dict[str, Any]:
        p = Path(checkpoint_path)
        if not p.is_file():
            return {"ok": False, "error": "checkpoint_missing"}
        with p.open("r", encoding="utf-8") as f:
            data = json.load(f)
        return {"ok": True, **data}

    def results_incremental(self, output_path: str | Path, record: Mapping[str, Any]) -> None:
        p = Path(output_path)
        p.parent.mkdir(parents=True, exist_ok=True)
        line = json.dumps(dict(record), ensure_ascii=False, default=str) + "\n"
        with p.open("a", encoding="utf-8") as f:
            f.write(line)

    def _iter_dataset(self, dataset_path: str | Path) -> Iterator[Dict[str, Any]]:
        p = Path(dataset_path)
        with p.open("r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    obj = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if isinstance(obj, dict):
                    yield obj

    def run_offline(
        self,
        model_path: str | Path,
        dataset_path: str | Path,
        output_path: str | Path,
        *,
        checkpoint_path: Optional[str | Path] = None,
        start_index: int = 0,
        score_fn: Optional[Any] = None,
    ) -> Dict[str, Any]:
        """
        Iterate JSONL rows (expects ``prompt`` or ``question``). Append one JSON result per row.
        Optional ``score_fn(prompt, response) -> dict``; default stub scores length only.
        """
        out_p = Path(output_path)
        ck_path = Path(checkpoint_path) if checkpoint_path else out_p.with_suffix(".checkpoint.json")
        rows: List[Dict[str, Any]] = list(self._iter_dataset(dataset_path))
        total = len(rows)
        done = max(0, int(start_index))
        interval = self.checkpoint_every(self._checkpoint_interval)

        def default_score(prompt: str, response: str) -> Dict[str, Any]:
            return {
                "ok": True,
                "prompt": prompt[:200],
                "response": response[:200],
                "stub_score": min(1.0, len(response) / 500.0),
            }

        sf = score_fn or default_score
        model_path_str = str(Path(model_path).expanduser())

        for i in range(done, total):
            row = rows[i]
            prompt = str(row.get("prompt") or row.get("question") or row.get("text", ""))
            response = str(row.get("response") or row.get("answer") or f"(offline_stub){i}")
            if "response" not in row or not str(row.get("response") or "").strip():
                response = f"(offline_stub){i}"
            rec = {"i": i, "model_path": model_path_str, **sf(prompt, response)}
            self.results_incremental(out_p, rec)
            if (i + 1) % interval == 0 or i == total - 1:
                ck = {
                    "next_index": i + 1,
                    "total": total,
                    "model_path": model_path_str,
                    "output_path": str(out_p),
                }
                ck_path.parent.mkdir(parents=True, exist_ok=True)
                ck_path.write_text(json.dumps(ck, indent=2), encoding="utf-8")

        return {
            "ok": True,
            "processed": total - done,
            "total": total,
            "output_path": str(out_p),
            "checkpoint_path": str(ck_path),
            "disclaimer": "No network; stub scoring unless score_fn is wired to your model.",
        }
