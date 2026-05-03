# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-contamination — benchmark contamination **stubs** (canary, overlap, peakedness).

Replace heuristics with your data-stewardship pipeline. Never treat AUROC as trustworthy if
provenance is unknown (see ``docs/CLAIM_DISCIPLINE.md``)."""
from __future__ import annotations

import math
from datetime import datetime
from typing import Any, Dict, List, Mapping, Sequence, Tuple

__all__ = ["SigmaContamination"]


class SigmaContamination:
    """Cheap overlap / distribution checks; ``model`` hooks are optional lab stubs."""

    @staticmethod
    def check_dataset(dataset: Sequence[Mapping[str, Any]], model: Any) -> Dict[str, Any]:
        del model
        texts = [str(r.get("text") or r.get("prompt") or r.get("input", "")) for r in dataset]
        dup = len(texts) - len(set(texts))
        return {
            "duplicate_rows": int(dup),
            "suspect_memorization": bool(dup > max(1, len(texts) // 20)),
            "note": "Wire real de-dup / shingle tests upstream.",
        }

    @staticmethod
    def canary_test(model: Any, canary_text: str) -> Dict[str, Any]:
        """Detects literal canary echo from ``model.generate`` or ``model(dummy)``."""
        canary = str(canary_text)
        out = ""
        if hasattr(model, "generate") and callable(model.generate):
            try:
                out = str(model.generate(canary))
            except Exception:
                out = ""
        elif callable(model):
            try:
                out = str(model(canary))
            except Exception:
                out = ""
        remembered = canary[:20].lower() in out.lower() if len(canary) >= 20 else canary.lower() in out.lower()
        return {"canary_used": canary[:40], "remembered": bool(remembered), "output_preview": out[:120]}

    @staticmethod
    def output_distribution_peakedness(model: Any, dataset: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        """Entropy of length buckets from ``model`` outputs (high = less peaked)."""
        del model
        lengths = [len(str(r.get("output") or r.get("response") or "")) for r in dataset]
        if not lengths:
            return {"peakedness_proxy": 0.0, "entropy": 0.0}
        buckets = [min(10, max(0, L // 20)) for L in lengths]
        freq: Dict[int, int] = {}
        for b in buckets:
            freq[b] = freq.get(b, 0) + 1
        n = len(buckets)
        ent = -sum((c / n) * math.log((c / n) + 1e-12) for c in freq.values())
        peaked = 1.0 - min(1.0, ent / math.log(max(len(freq), 2)))
        return {"peakedness_proxy": round(float(peaked), 6), "entropy": round(float(ent), 6)}

    @staticmethod
    def temporal_filter(
        dataset: Sequence[Mapping[str, Any]],
        model_training_cutoff: str,
    ) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
        """Split rows with ``timestamp`` ISO field before/after cutoff (inclusive after)."""
        cutoff = datetime.fromisoformat(str(model_training_cutoff).replace("Z", "+00:00"))
        kept, dropped = [], []
        for row in dataset:
            ts = row.get("timestamp")
            if ts is None:
                kept.append(dict(row))
                continue
            t = datetime.fromisoformat(str(ts).replace("Z", "+00:00"))
            (kept if t >= cutoff else dropped).append(dict(row))
        return kept, dropped

    def clean_benchmark(self, dataset: Sequence[Mapping[str, Any]], model: Any) -> Dict[str, Any]:
        chk = self.check_dataset(dataset, model)
        mem = chk.get("suspect_memorization", False)
        clean = [dict(r) for r in dataset]
        removed = 0
        if mem and clean:
            seen: set[str] = set()
            new_clean: List[Dict[str, Any]] = []
            for r in clean:
                key = str(r.get("text") or r.get("id", ""))
                if key in seen:
                    removed += 1
                    continue
                seen.add(key)
                new_clean.append(r)
            clean = new_clean
        return {"clean": clean, "removed": removed, "check": chk}

    def report(self, dataset: Sequence[Mapping[str, Any]], model: Any) -> Dict[str, Any]:
        chk = self.check_dataset(dataset, model)
        peak = self.output_distribution_peakedness(model, dataset)
        frac = 0.05 if chk.get("suspect_memorization") else 0.0
        frac = max(frac, peak["peakedness_proxy"] * 0.1)
        return {
            "possibly_contaminated_fraction": round(float(min(1.0, frac)), 4),
            "details": {"check": chk, "distribution": peak},
            "disclaimer": "Illustrative fraction — not a certified audit.",
        }
