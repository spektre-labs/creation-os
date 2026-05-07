# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Cross-domain transfer with compositional HDC bindings and σ validation (lab).

Primitive combinations use hypervector **bind** when :class:`~cos.hypervector.SigmaHDC`
is supplied; the σ-gate scores analogy, composition, and rule transfer strings.

**NOT AGI ACHIEVED** — structural analogy hooks only; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence

__all__ = ["SigmaTransfer"]


class SigmaTransfer:
    """Transfer learning via compositional encoding (optional HDC) and σ-validation."""

    def __init__(self, gate: Any = None, hdc: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.hdc = hdc
        self.domains: Dict[str, Dict[str, Any]] = {}
        self.transfers: List[Dict[str, Any]] = []

    def register_domain(
        self,
        name: str,
        primitives: Sequence[str],
        rules: Optional[Sequence[str]] = None,
    ) -> None:
        """Register a domain with explicit primitives (and optional rule strings)."""
        self.domains[str(name)] = {
            "primitives": [str(p) for p in primitives],
            "rules": [str(r) for r in rules] if rules is not None else [],
            "σ_avg": 0.5,
        }

    def find_analogous(self, source_domain: str, target_domain: str) -> List[Dict[str, Any]]:
        """Pair primitives across domains when the gate scores them as similar (σ < 0.5)."""
        source = self.domains.get(source_domain, {})
        target = self.domains.get(target_domain, {})
        if not source or not target:
            return []

        analogies: List[Dict[str, Any]] = []
        for sp in source.get("primitives", []):
            for tp in target.get("primitives", []):
                σ, _verdict = self.gate.score(
                    f"{sp} in {source_domain}",
                    f"{tp} in {target_domain}",
                )
                σ = float(σ)
                if σ < 0.5:
                    analogies.append(
                        {
                            "source": sp,
                            "target": tp,
                            "σ": round(σ, 4),
                        }
                    )
        analogies.sort(key=lambda x: x["σ"])
        return analogies

    def _hdc_codebook(self) -> Any:
        if self.hdc is None:
            return None
        book = getattr(self.hdc, "book", None)
        if book is not None:
            return book
        return getattr(self.hdc, "codebook", None)

    def compose_novel(
        self,
        primitives: Sequence[str],
        operation: str = "combine",
    ) -> Dict[str, Any]:
        """Compose primitives into a novel string; optional HDC ``bind`` chain; σ from the gate."""
        prim_list = [str(p) for p in primitives]
        composition = f"{operation}({', '.join(prim_list)})"
        σ, verdict = self.gate.score(
            f"known primitives: {prim_list}",
            f"novel composition: {composition}",
        )
        σ = float(σ)
        verdict_s = str(verdict)
        result: Dict[str, Any] = {
            "composition": composition,
            "primitives": prim_list,
            "σ": round(σ, 4),
            "verdict": verdict_s,
            "novel": True,
            "coherent": verdict_s != "ABSTAIN",
        }

        book = self._hdc_codebook()
        if book is not None:
            try:
                from cos.hypervector import HyperVector
            except ImportError:
                result["hdc_encoded"] = False
                return result

            encoded = [book[p] for p in prim_list]
            if len(encoded) >= 2:
                composed = HyperVector.bind(encoded[0], encoded[1])
                for e in encoded[2:]:
                    composed = HyperVector.bind(composed, e)
                result["hdc_encoded"] = True
                result["composed_dim"] = int(composed.dim)
            else:
                result["hdc_encoded"] = False
        return result

    def transfer(self, source_domain: str, target_domain: str, rule: str) -> Dict[str, Any]:
        """Map a source rule to the target domain via analogy pairs; σ validates the mapping."""
        analogies = self.find_analogous(source_domain, target_domain)
        if not analogies:
            return {
                "success": False,
                "σ": 1.0,
                "reason": "no analogies found",
            }

        best_map: Dict[str, str] = {}
        for a in analogies:
            s = str(a["source"])
            if s not in best_map:
                best_map[s] = str(a["target"])

        mapped_rule = str(rule)
        for src in sorted(best_map.keys(), key=len, reverse=True):
            mapped_rule = mapped_rule.replace(src, best_map[src])

        σ, verdict = self.gate.score(
            f"rule in {source_domain}: {rule}",
            f"transferred to {target_domain}: {mapped_rule}",
        )
        σ = float(σ)
        verdict_s = str(verdict)
        result = {
            "source_domain": source_domain,
            "target_domain": target_domain,
            "original_rule": str(rule),
            "transferred_rule": mapped_rule,
            "analogies_used": len(analogies),
            "σ": round(σ, 4),
            "verdict": verdict_s,
            "success": verdict_s != "ABSTAIN",
        }
        self.transfers.append(result)
        return result

    def transfer_score(self) -> float:
        """Fraction of recorded transfers with non-ABSTAIN verdict."""
        if not self.transfers:
            return 0.0
        successes = sum(1 for t in self.transfers if t.get("success"))
        return round(successes / len(self.transfers), 4)
