# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Few-shot via **hyperdimensional computing** (encode → bundle → similarity) + σ readout.

No gradients / fine-tuning. When NumPy-backed :mod:`cos.hypervector` is unavailable, falls back
to **word-overlap** prototypes. Optional :class:`~cos.memory.SigmaMemory` recall can populate
support sets. See ``docs/CLAIM_DISCIPLINE.md`` — **not** MEMHD / CUB-200 benchmark claims."""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence, Tuple

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaFewShot"]

try:
    from cos.hypervector import HDCodebook, HyperVector, SigmaHDC  # noqa: F401

    _probe = HyperVector.from_seed(64, "fewshot_hdc_probe")
    del _probe
    _HAS_HDC = True
except Exception:  # pragma: no cover - numpy missing or hypervector init failure
    HyperVector = None  # type: ignore[misc, assignment]
    HDCodebook = None  # type: ignore[misc, assignment]
    SigmaHDC = None  # type: ignore[misc, assignment]
    _HAS_HDC = False


class SigmaFewShot:
    """Few-shot class prototypes: **HDC bundle** or **overlap**; σ from similarity bands."""

    def __init__(
        self,
        gate: Any = None,
        dim: int = 10_000,
        *,
        use_hdc: Optional[bool] = None,
        codebook_seed: int = 0,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.dim = int(dim)
        self._use_hdc = bool(_HAS_HDC) if use_hdc is None else bool(use_hdc)
        self.classes: Dict[str, Dict[str, Any]] = {}
        self.codebook: Optional[Any] = None
        if self._use_hdc and HDCodebook is not None:
            self.codebook = HDCodebook(dim=self.dim, seed=int(codebook_seed))

    def learn(self, class_name: str, examples: Sequence[str]) -> Dict[str, Any]:
        """Build a class prototype from N text supports (bundle encodings or overlap store)."""
        cname = str(class_name)
        ex = [str(x) for x in examples]
        if not ex:
            return {"class": cname, "n_examples": 0, "method": "none", "learned": False}

        if not self._use_hdc or self.codebook is None or HyperVector is None:
            self.classes[cname] = {"examples": ex, "method": "overlap"}
            return {"class": cname, "n_examples": len(ex), "method": "overlap", "learned": True}

        encoded = [self._encode_text(e) for e in ex]
        prototype = HyperVector.bundle(encoded) if len(encoded) > 1 else encoded[0]
        self.classes[cname] = {
            "prototype": prototype,
            "n_examples": len(ex),
            "method": "hdc",
        }
        return {"class": cname, "n_examples": len(ex), "method": "hdc", "learned": True}

    def classify(self, query: str) -> Dict[str, Any]:
        """Best prototype match; σ maps from **dissimilarity** (+ gate thresholds)."""
        if not self.classes:
            return {
                "class": None,
                "σ": 1.0,
                "verdict": "ABSTAIN",
                "reason": "no classes learned",
            }

        if self._use_hdc and HyperVector is not None:
            return self._classify_hdc(str(query))
        return self._classify_overlap(str(query))

    def adapt(
        self,
        examples_with_labels: Sequence[Tuple[str, str, bool]],
        gate: Optional[Any] = None,
    ) -> Dict[str, Any]:
        """Calibration **report** over labeled rows (does not mutate gate thresholds in lite mode)."""
        g = gate or self.gate
        σ_before: List[float] = []
        σ_after: List[float] = []
        for row in examples_with_labels:
            prompt, response, _correct = row[0], row[1], row[2]
            σ, _ = g.score(str(prompt), str(response))
            σ_before.append(float(σ))
            # Lite lab: re-score same pair (placeholder for threshold learning)
            σ_new, _ = g.score(str(prompt), str(response))
            σ_after.append(float(σ_new))

        n = len(σ_before)
        avg_b = sum(σ_before) / max(n, 1)
        avg_a = sum(σ_after) / max(n, 1)
        return {
            "n_examples": len(examples_with_labels),
            "σ_before_avg": round(avg_b, 4),
            "σ_after_avg": round(avg_a, 4),
            "improved": bool(avg_a < avg_b),
        }

    def transfer_check(
        self,
        source_examples: Sequence[str],
        target_examples: Sequence[str],
    ) -> Dict[str, Any]:
        """Compare mean classification σ before vs after ``learn('source', ...)``."""
        targs = [str(t) for t in target_examples]
        σ_before: List[float] = []
        for ex in targs:
            result = self.classify(ex)
            σ_before.append(float(result.get("σ", 1.0)))

        self.learn("source", list(source_examples))

        σ_after: List[float] = []
        for ex in targs:
            result = self.classify(ex)
            σ_after.append(float(result.get("σ", 1.0)))

        avg_before = sum(σ_before) / max(len(σ_before), 1)
        avg_after = sum(σ_after) / max(len(σ_after), 1)
        return {
            "σ_before": round(avg_before, 4),
            "σ_after": round(avg_after, 4),
            "transferred": bool(avg_after < avg_before),
            "improvement": round(avg_before - avg_after, 4),
        }

    def recall_prototype_support(
        self,
        memory: Any,
        query: str,
        class_name: str,
        *,
        top_k: int = 5,
    ) -> Dict[str, Any]:
        """Wire :class:`~cos.memory.SigmaMemory` recall into :meth:`learn` (lab convenience)."""
        recall_fn = getattr(memory, "recall", None)
        if not callable(recall_fn):
            return {"learned": False, "reason": "memory.recall missing"}
        rows = recall_fn(str(query), top_k=int(top_k))
        examples: List[str] = []
        for r in rows:
            if isinstance(r, dict):
                examples.append(str(r.get("content", "")))
            else:
                examples.append(str(r))
        examples = [e for e in examples if e.strip()]
        if not examples:
            return {"learned": False, "reason": "no recall rows"}
        return self.learn(class_name, examples)

    def _encode_text(self, text: str) -> Any:
        assert self.codebook is not None and HyperVector is not None
        tokens = str(text).lower().split()
        if not tokens:
            return HyperVector.from_seed(self.dim, "__empty__")
        vecs: List[Any] = [self.codebook[tokens[0]]]
        for i, token in enumerate(tokens[1:], start=1):
            vecs.append(HyperVector.permute(self.codebook[token], i))
        return HyperVector.bundle(vecs) if len(vecs) > 1 else vecs[0]

    def _classify_hdc(self, query: str) -> Dict[str, Any]:
        assert HyperVector is not None
        query_hv = self._encode_text(query)
        best_class: Optional[str] = None
        best_sim = -2.0
        scores: Dict[str, float] = {}

        for cls_name, cls_data in self.classes.items():
            if cls_data.get("method") != "hdc":
                continue
            proto = cls_data.get("prototype")
            if proto is None:
                continue
            sim = HyperVector.similarity(query_hv, proto)
            scores[cls_name] = sim
            if sim > best_sim:
                best_sim = sim
                best_class = cls_name

        if best_class is None:
            return {
                "class": None,
                "σ": 1.0,
                "verdict": "ABSTAIN",
                "reason": "no hdc prototypes",
            }

        σ = max(0.0, min(1.0, (1.0 - best_sim) / 2.0))
        ta, tb = float(self.gate.threshold_accept), float(self.gate.threshold_abstain)
        if σ < ta:
            verdict = "ACCEPT"
        elif σ > tb:
            verdict = "ABSTAIN"
        else:
            verdict = "RETHINK"

        return {
            "class": best_class,
            "similarity": round(float(best_sim), 4),
            "σ": round(float(σ), 4),
            "verdict": verdict,
            "all_scores": {k: round(float(v), 4) for k, v in scores.items()},
        }

    def _classify_overlap(self, query: str) -> Dict[str, Any]:
        query_words = set(query.lower().split())
        best_class: Optional[str] = None
        best_overlap = -1.0

        for cls_name, cls_data in self.classes.items():
            examples = cls_data.get("examples", [])
            if not examples:
                continue
            total_overlap = 0.0
            for ex in examples:
                ex_words = set(str(ex).lower().split())
                total_overlap += float(len(query_words & ex_words))
            avg = total_overlap / float(len(examples))
            if avg > best_overlap:
                best_overlap = avg
                best_class = cls_name

        denom = max(len(query_words), 1)
        σ = max(0.0, min(1.0, 1.0 - best_overlap / denom)) if best_class else 1.0
        ta, tb = float(self.gate.threshold_accept), float(self.gate.threshold_abstain)
        if σ < ta:
            verdict = "ACCEPT"
        elif σ > tb:
            verdict = "ABSTAIN"
        else:
            verdict = "RETHINK"

        return {
            "class": best_class,
            "σ": round(float(σ), 4),
            "verdict": verdict,
        }
