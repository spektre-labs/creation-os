# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-speculative — draft + verify with ``SigmaGate`` as the acceptance criterion.

**Decode path:** cheap draft tokens are scored with :meth:`SigmaGate.score`; ACCEPT skips
target verification, RETHINK runs ``verify_fn``, ABSTAIN hands off to the target. Legacy
helpers (draft length from σ, prefix verify, tree draft) remain for harness experiments.
No throughput claims without host metadata; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Sequence, Union

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaSpeculative"]

DraftModel = Union[Callable[..., List[str]], Any]


def _norm_verdict(v: Any) -> str:
    raw = str(getattr(v, "name", v))
    return raw.split(".")[-1] if "." in raw else raw


class SigmaSpeculative:
    """Draft tokens; σ-gate decides accept vs verify vs target takeover."""

    def __init__(
        self,
        gate: Any = None,
        *,
        min_draft: int = 1,
        max_draft: int = 8,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.min_draft = max(1, int(min_draft))
        self.max_draft = max(self.min_draft, int(max_draft))
        self.stats: Dict[str, int] = {
            "drafted": 0,
            "accepted": 0,
            "verified": 0,
            "rejected": 0,
            "output_tokens": 0,
        }

    def decode(
        self,
        prompt: str,
        draft_fn: Callable[[str, int], Sequence[str]],
        verify_fn: Callable[[str, Optional[str]], Optional[str]],
        max_tokens: int = 100,
        k: int = 5,
    ) -> Dict[str, Any]:
        """Draft → σ-score → accept / verify / target; σ is the acceptance rule."""
        self.stats = {
            "drafted": 0,
            "accepted": 0,
            "verified": 0,
            "rejected": 0,
            "output_tokens": 0,
        }
        output: List[Dict[str, Any]] = []
        current_prompt = str(prompt)
        cap = max(0, int(max_tokens))
        k = max(1, int(k))

        while len(output) < cap:
            drafts = list(draft_fn(current_prompt, k))
            if not drafts:
                break

            accepted_in_batch = 0
            for draft_token in drafts:
                if len(output) >= cap:
                    break
                dt = str(draft_token).strip()
                if not dt:
                    continue
                self.stats["drafted"] += 1
                σ, verdict = self.gate.score(current_prompt, dt)
                v = _norm_verdict(verdict)

                if v == "ACCEPT":
                    output.append(
                        {
                            "token": dt,
                            "σ": float(σ),
                            "source": "draft",
                            "verified": False,
                        }
                    )
                    current_prompt = f"{current_prompt} {dt}".strip()
                    self.stats["accepted"] += 1
                    self.stats["output_tokens"] += 1
                    accepted_in_batch += 1
                elif v == "RETHINK":
                    self.stats["verified"] += 1
                    verified = verify_fn(current_prompt, dt)
                    if verified:
                        vt = str(verified).strip()
                        output.append(
                            {
                                "token": vt,
                                "σ": float(σ),
                                "source": "verified",
                                "verified": True,
                            }
                        )
                        current_prompt = f"{current_prompt} {vt}".strip()
                        self.stats["output_tokens"] += 1
                        accepted_in_batch += 1
                    else:
                        self.stats["rejected"] += 1
                        break
                else:
                    self.stats["rejected"] += 1
                    self.stats["verified"] += 1
                    verified = verify_fn(current_prompt, None)
                    if verified:
                        vt = str(verified).strip()
                        output.append(
                            {
                                "token": vt,
                                "σ": float(σ),
                                "source": "target",
                                "verified": True,
                            }
                        )
                        current_prompt = f"{current_prompt} {vt}".strip()
                        self.stats["output_tokens"] += 1
                        accepted_in_batch += 1
                    break

            if len(output) >= cap:
                break
            if accepted_in_batch == 0:
                self.stats["verified"] += 1
                verified = verify_fn(current_prompt, None)
                if verified:
                    vt = str(verified).strip()
                    output.append(
                        {
                            "token": vt,
                            "σ": 0.0,
                            "source": "target_fallback",
                            "verified": True,
                        }
                    )
                    current_prompt = f"{current_prompt} {vt}".strip()
                    self.stats["output_tokens"] += 1
                else:
                    break

        return {
            "tokens": output,
            "text": " ".join(str(t["token"]) for t in output),
            "stats": self.efficiency(),
        }

    def efficiency(self) -> Dict[str, Any]:
        """Rough compute mix: draft 1×, verify / target steps 10× (lab estimate only)."""
        total = int(self.stats["drafted"])
        if total == 0:
            return {
                "draft_accept_rate": 0.0,
                "verify_rate": 0.0,
                "reject_rate": 0.0,
                "speedup_estimate": 1.0,
                "tokens_generated": int(self.stats.get("output_tokens", 0)),
            }

        draft_rate = self.stats["accepted"] / total
        verify_rate = self.stats["verified"] / total
        reject_rate = self.stats["rejected"] / total
        draft_cost = total * 1
        verify_cost = self.stats["verified"] * 10
        target_cost = self.stats["rejected"] * 10
        total_cost = draft_cost + verify_cost + target_cost
        baseline_cost = total * 10
        speedup = baseline_cost / max(total_cost, 1)
        out_tok = int(self.stats.get("output_tokens", 0))
        return {
            "draft_accept_rate": round(draft_rate, 4),
            "verify_rate": round(verify_rate, 4),
            "reject_rate": round(reject_rate, 4),
            "speedup_estimate": round(float(speedup), 2),
            "tokens_generated": out_tok,
        }

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
