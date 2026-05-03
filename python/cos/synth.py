# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-synth — synthetic example generation with σ filtering (lab, fights trivial self-copy).

Does **not** implement AGORABENCH harnesses; naming is illustrative. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import statistics
from typing import Any, Dict, List, Mapping, Sequence, Set, Tuple

__all__ = ["SigmaSynth"]


class SigmaSynth:
    """Generate text from a ``model``, filter by σ, check diversity / benchmark overlap."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()

    def generate(self, model: Any, seed_prompts: Sequence[str], n: int) -> List[Dict[str, Any]]:
        """Call ``model.generate(prompt) -> str`` per seed (round-robin); lab echo fallback."""
        seeds = [str(s) for s in seed_prompts]
        if not seeds:
            return []
        n = max(0, int(n))
        out: List[Dict[str, Any]] = []
        gen_fn = getattr(model, "generate", None)
        for i in range(n):
            sp = seeds[i % len(seeds)]
            if callable(gen_fn):
                text = str(gen_fn(sp))
            else:
                text = f"(stub){sp}::{i}"
            out.append({"prompt": sp, "response": text, "id": i})
        return out

    def sigma_filter(
        self,
        examples: Sequence[Mapping[str, Any]],
        gate: Any,
        threshold: float,
    ) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
        g = gate or self.gate
        t = float(threshold)
        kept: List[Dict[str, Any]] = []
        removed: List[Dict[str, Any]] = []
        for ex in examples:
            p, r = str(ex.get("prompt", "")), str(ex.get("response", ""))
            sigma, verdict = g.score(p, r)
            row = {**dict(ex), "sigma": float(sigma), "verdict": str(verdict)}
            if float(sigma) <= t:
                kept.append(row)
            else:
                removed.append(row)
        return kept, removed

    @staticmethod
    def diversity_score(examples: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        """Mean pairwise Jaccard distance on word multisets (crude diversity)."""

        def words(s: str) -> Set[str]:
            return {w.lower() for w in s.split() if w.isalnum() or "_" in w}

        texts = [str(ex.get("response", "")) for ex in examples]
        sets = [words(t) for t in texts if t.strip()]
        if len(sets) < 2:
            return {"diversity": 0.0, "n": len(sets)}
        dists: List[float] = []
        for i in range(len(sets)):
            for j in range(i + 1, len(sets)):
                a, b = sets[i], sets[j]
                inter = len(a & b)
                union = len(a | b) or 1
                dists.append(1.0 - inter / union)
        return {"diversity": round(float(statistics.mean(dists)), 6), "n_pairs": len(dists)}

    @staticmethod
    def contamination_check(
        examples: Sequence[Mapping[str, Any]],
        benchmark_data: Sequence[str],
    ) -> Dict[str, Any]:
        """Substring overlap vs benchmark strings — flag possible leakage (toy)."""
        bench_blob = "\n".join(str(x)[:5000] for x in benchmark_data).lower()
        flagged: List[int] = []
        for ex in examples:
            rid = int(ex.get("id", -1))
            r = str(ex.get("response", "")).lower().strip()
            if len(r) >= 24 and r[:32] in bench_blob:
                flagged.append(rid)
            for needle in benchmark_data:
                n = str(needle).lower().strip()
                if len(n) >= 16 and n in r:
                    flagged.append(rid)
                    break
        return {"flagged_ids": sorted(set(flagged)), "n_flagged": len(set(flagged))}

    @staticmethod
    def quality_report(filtered: Sequence[Mapping[str, Any]], removed: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        sig_k = [float(x.get("sigma", 0.5)) for x in filtered]
        sig_r = [float(x.get("sigma", 0.5)) for x in removed]
        return {
            "n_generated": len(filtered) + len(removed),
            "n_accepted": len(filtered),
            "n_removed": len(removed),
            "avg_sigma_filtered": round(float(statistics.mean(sig_k)), 6) if sig_k else 0.0,
            "avg_sigma_removed": round(float(statistics.mean(sig_r)), 6) if sig_r else 0.0,
        }

    def iterative_refinement(
        self,
        examples: Sequence[Mapping[str, Any]],
        gate: Any,
        rounds: int,
        *,
        threshold: float = 0.45,
        refine_suffix: str = " [clarity:v2]",
    ) -> Dict[str, Any]:
        """Filter repeatedly; append suffix to prompts of kept rows and re-score (lab loop)."""
        g = gate or self.gate
        current = [dict(x) for x in examples]
        history: List[Dict[str, Any]] = []
        kept: List[Dict[str, Any]] = []
        removed: List[Dict[str, Any]] = []
        for _round in range(max(1, int(rounds))):
            kept, removed = self.sigma_filter(current, g, threshold)
            history.append({"round": _round + 1, "kept": len(kept), "removed": len(removed)})
            if not kept or _round == rounds - 1:
                return {"final_kept": kept, "final_removed": removed, "history": history}
            current = []
            for row in kept:
                current.append(
                    {
                        **row,
                        "prompt": str(row.get("prompt", "")) + refine_suffix,
                        "response": str(row.get("response", "")),
                    },
                )
        return {"final_kept": kept, "final_removed": removed, "history": history}
