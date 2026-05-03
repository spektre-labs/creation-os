# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-speculative — lab sketch for draft-then-verify with ``SigmaGate`` as the verifier.

Production stacks combine draft and target models; here the gate stands in for a target
**accept/reject** signal on cumulative prefixes (same lite entropy kernel as
:class:`cos.stream.SigmaStream`). No throughput or speedup claims; see
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Sequence, Union

__all__ = ["SigmaSpeculative"]

DraftModel = Union[Callable[..., List[str]], Any]


class SigmaSpeculative:
    """Draft tokens, verify per prefix with σ, optional target fallback (callable)."""

    def __init__(
        self,
        *,
        min_draft: int = 1,
        max_draft: int = 8,
    ) -> None:
        self.min_draft = max(1, int(min_draft))
        self.max_draft = max(self.min_draft, int(max_draft))

    def draft(
        self,
        prompt: str,
        draft_model: DraftModel,
        n_tokens: int,
    ) -> List[str]:
        n = max(0, int(n_tokens))
        if n == 0:
            return []
        if callable(draft_model):
            out = draft_model(str(prompt), n)
            return [str(t) for t in (out if isinstance(out, list) else list(out))][:n]
        d = getattr(draft_model, "draft", None)
        if callable(d):
            out = d(str(prompt), n)
            return [str(t) for t in out][:n]
        words = str(prompt).split()
        return words[:n] if words else ["<eos>"][: min(1, n)]

    def verify(
        self,
        prompt: str,
        draft_tokens: Sequence[str],
        gate: Any,
    ) -> Dict[str, Any]:
        accepted: List[str] = []
        prefix = ""
        rejected_at: Optional[int] = None
        sigmas: List[float] = []
        for i, tok in enumerate(draft_tokens):
            prefix = (prefix + " " + str(tok)).strip()
            sigma = float(gate.compute_sigma(None, None, str(prompt), prefix))
            sigmas.append(sigma)
            verdict = str(gate._verdict(sigma))
            if verdict == "ACCEPT":
                accepted.append(str(tok))
            else:
                rejected_at = i
                break
        denom = max(len(draft_tokens), 1)
        return {
            "accepted": accepted,
            "rejected_at": rejected_at,
            "sigmas": sigmas,
            "accept_ratio": len(accepted) / denom,
        }

    def draft_length_from_sigma(self, sigma: float) -> int:
        """Lower σ (calmer prefix) → longer speculative run; high σ → shorter."""
        s = max(0.0, min(1.0, float(sigma)))
        span = self.max_draft - self.min_draft
        return self.max_draft - int(round(s * span))

    def speculative_generate(
        self,
        prompt: str,
        draft_model: DraftModel,
        target_model: DraftModel,
        gate: Any,
    ) -> Dict[str, Any]:
        seed_sigma = float(gate.compute_sigma(None, None, str(prompt), ""))
        n = self.draft_length_from_sigma(seed_sigma)
        draft_tokens = self.draft(prompt, draft_model, n)
        ver = self.verify(prompt, draft_tokens, gate)
        if ver["rejected_at"] is None:
            return {
                "text": " ".join(ver["accepted"]),
                "mode": "draft_only",
                "verification": ver,
            }
        tail = self._call_target(target_model, prompt, ver["accepted"])
        body = " ".join(ver["accepted"])
        if tail:
            text = f"{body} {tail}".strip()
        else:
            text = body
        return {
            "text": text,
            "mode": "draft_plus_target",
            "verification": ver,
        }

    def tree_draft(
        self,
        prompt: str,
        draft_model: DraftModel,
        *,
        branches: int = 3,
        depth: int = 2,
    ) -> Dict[str, Any]:
        """Medusa-style lab tree: one root draft row plus shallow child rows."""
        br = max(1, int(branches))
        dp = max(1, int(depth))
        root = self.draft(prompt, draft_model, br)
        children: List[List[str]] = []
        for tok in root[:br]:
            child_prompt = f"{prompt} {tok}"
            children.append(self.draft(child_prompt, draft_model, dp))
        return {"root": root, "children": children, "branches": br, "depth": dp}

    @staticmethod
    def _call_target(
        target_model: DraftModel,
        prompt: str,
        prefix_tokens: List[str],
    ) -> str:
        ctx = f"{prompt} | " + " ".join(prefix_tokens)
        if callable(target_model):
            out = target_model(ctx, 8)
            if isinstance(out, list):
                return " ".join(str(x) for x in out)
            return str(out)
        gen = getattr(target_model, "generate", None)
        if callable(gen):
            return str(gen(ctx))
        return ""
