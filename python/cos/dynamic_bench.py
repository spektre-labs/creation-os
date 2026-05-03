# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-dynamic-bench — versioned synthetic question pools (lab).

Not a replacement for curated evaluations. Any “static vs dynamic” comparison here is
**structural scaffolding** only — do not cite as measured AUROC. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Mapping, Optional, Sequence

__all__ = ["SigmaDynamicBench"]


class SigmaDynamicBench:
    """Generate template questions, σ-check them, bucket σ tiers, store snapshots."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self._snapshots: List[Dict[str, Any]] = []

    def generate_questions(self, domain: str, difficulty: str, n: int) -> List[Dict[str, Any]]:
        """Produce ``n`` lab questions — explicit tags, not scraped benchmark items."""
        n = max(0, int(n))
        dom, diff = str(domain), str(difficulty)
        return [
            {
                "domain": dom,
                "difficulty": diff,
                "question": (
                    f"[dynamic-bench:{dom}:{diff}] Synthetic evaluation probe #{i + 1} "
                    "(operator-generated; treat distribution as novel)."
                ),
            }
            for i in range(n)
        ]

    def sigma_validate_questions(
        self,
        questions: Sequence[Mapping[str, Any] | str],
        gate: Optional[Any] = None,
        *,
        threshold: float = 0.65,
    ) -> Dict[str, Any]:
        """σ-gate the **question text** (cheap self-contradiction / emptiness proxy)."""
        g = gate or self.gate
        t = float(threshold)
        items: List[Dict[str, Any]] = []
        for q in questions:
            text = str(q["question"]) if isinstance(q, Mapping) else str(q)
            sigma, verdict = g.score("dynamic_bench_question", text)
            ok = float(sigma) < t
            items.append({"question": text, "sigma": float(sigma), "verdict": str(verdict), "ok": ok})
        return {"items": items, "n_ok": sum(1 for x in items if x["ok"]), "n": len(items)}

    def run(self, model: Any, gate: Any, questions: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        """Aggregate σ into coarse M-tier counts after ``model`` answers each question."""
        g = gate or self.gate
        tiers = {"M0": 0, "M1": 0, "M2": 0, "M3": 0}
        rows: List[Dict[str, Any]] = []
        ans_fn = getattr(model, "answer", None)
        gen_fn = getattr(model, "generate", None)
        for q in questions:
            qt = str(q.get("question", ""))
            if callable(ans_fn):
                ans = str(ans_fn(qt))
            elif callable(gen_fn):
                ans = str(gen_fn(qt))
            else:
                ans = f"(stub_answer){qt[:24]}"
            sigma, verdict = g.score(qt, ans)
            s = float(sigma)
            bucket = "M0" if s < 0.25 else "M1" if s < 0.5 else "M2" if s < 0.75 else "M3"
            tiers[bucket] += 1
            rows.append({"sigma": s, "verdict": str(verdict), "m_tier": bucket})
        return {"m_tier_counts": tiers, "rows": rows, "n": len(questions)}

    def compare_static_vs_dynamic(self, model: Any, gate: Any) -> Dict[str, Any]:
        """Side-by-side **lab** summaries — interpret with ``contamination`` + provenance."""
        static_q = self.generate_questions("general", "easy", 4)
        dynamic_q = self.generate_questions("general", "hard", 4)
        static_run = self.run(model, gate, static_q)
        dynamic_run = self.run(model, gate, dynamic_q)
        return {
            "static_pool_run": static_run,
            "dynamic_pool_run": dynamic_run,
            "interpretation_lab": (
                "If easy static-template responses cluster in low-σ tiers while hard-dynamic items "
                "do not, treat static headline metrics as potentially inflated — validate with "
                "provenance and held-out dynamic sets."
            ),
        }

    def versioned_snapshots(self, questions: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        """Append an immutable snapshot id for audit replay."""
        snap = {"version": len(self._snapshots) + 1, "questions": [dict(q) for q in questions]}
        self._snapshots.append(snap)
        return snap

    def difficulty_gradient(self, model: Any, gate: Any, domain: str = "science") -> Dict[str, Any]:
        """easy → expert: where σ tiers shift (shape only; not a calibrated IRT curve)."""
        curve: List[Dict[str, Any]] = []
        for diff in ("easy", "medium", "hard", "expert"):
            qs = self.generate_questions(domain, diff, 2)
            rr = self.run(model, gate, qs)
            curve.append({"difficulty": diff, "run": rr})
        return {"domain": domain, "curve": curve}
