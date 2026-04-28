# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-router: tiered routing from **pre-generation risk** via the cognitive interrupt
(``sigma_gate_core`` / ``sigma_gate.h``).

Motivation: HALT-style *route vs. compute* framing (arXiv:2601.14210) and retrieval
quality awareness (e.g. Tiny-Critic RAG, arXiv:2603.00846) as **orientation only**.
This module is a **policy scaffold**: inject ``rag_fn`` / ``strong_model_fn`` for your
infra; default stubs keep the repo self-contained.

**Claim discipline:** token rates and ``calculate_savings`` are **illustrative**
what-if math for dashboards and eval harnesses — not live billing, not audited
provider quotes, and not a promise of production savings. Override rates via
``COS_TOKEN_PRICING_JSON`` (JSON object: model id → USD per 1K tokens). See
``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import json
import os
import warnings
from dataclasses import dataclass, field
from typing import Any, Callable, Dict, Mapping, Optional

from .sigma_gate_core import SIGMA_HALF, SigmaState, Verdict, sigma_gate, sigma_update

# Table version label only (operators should refresh rates from their contracts).
PRICING_AS_OF = "2026-04"


def default_token_pricing_usd_per_1k() -> Dict[str, float]:
    """
    Illustrative USD **per 1K tokens** (input+output not split — lab convenience).

    ``local_hosted`` is a tiny non-zero placeholder for marginal self-host cost
    (power, amortized silicon); set to 0.0 in custom tables if you only want API rows.
    """
    return {
        "local": 0.0,
        "local_hosted": 5.0e-5,
        "gpt2": 0.0,
        "gemma-2b": 0.0,
        "phi-4-mini": 0.0,
        "gpt-4o-mini": 0.00015,
        "gpt-4o": 0.0025,
        "claude-sonnet": 0.003,
        "claude-opus": 0.015,
        "gpt-4-turbo": 0.01,
        "abstain": 0.0,
        "_default_api": 0.001,
    }


def load_token_pricing_usd_per_1k() -> Dict[str, float]:
    """Merge defaults with optional ``COS_TOKEN_PRICING_JSON`` (object of str → float)."""
    out = default_token_pricing_usd_per_1k()
    raw = (os.environ.get("COS_TOKEN_PRICING_JSON") or "").strip()
    if raw:
        try:
            extra = json.loads(raw)
            if isinstance(extra, dict):
                for k, v in extra.items():
                    if isinstance(k, str) and isinstance(v, (int, float)):
                        out[str(k)] = float(v)
        except json.JSONDecodeError:
            pass
    return out


def estimate_cost(
    model_name: str,
    tokens: int,
    *,
    pricing: Optional[Mapping[str, float]] = None,
) -> float:
    """USD estimate: ``(USD per 1K) * tokens / 1000`` (negative tokens treated as 0)."""
    table: Mapping[str, float] = pricing if pricing is not None else load_token_pricing_usd_per_1k()
    key = (model_name or "").strip() or "_default_api"
    rate = float(table.get(key, table.get("_default_api", 0.001)))
    return rate * max(0, int(tokens)) / 1000.0


_TIER_MODEL_FOR_SAVINGS = {
    "fast": "local_hosted",
    "verify": "local_hosted",
    "rag": "gpt-4o-mini",
    "escalate": "gpt-4o",
    "abstain": "abstain",
}


def calculate_savings(
    total_queries: int,
    routing_stats: Mapping[str, Any],
    *,
    avg_tokens_per_query: int = 300,
    baseline_model: str = "gpt-4o",
    pricing: Optional[Mapping[str, float]] = None,
) -> Dict[str, Any]:
    """
    Compare an **all-baseline-model** budget vs a tiered mix using **observed route
    fractions** from ``routing_stats`` (``get_stats()`` / ``router_summary.json``).

    ``total_queries`` scales both sides; distribution comes from ``routing_stats["total"]``
    and per-tier counts. If ``total`` is missing or zero, returns zeros and a note.
    """
    n_obs = int(routing_stats.get("total", 0) or 0)
    tq = max(0, int(total_queries))
    p = dict(pricing) if pricing is not None else load_token_pricing_usd_per_1k()
    note: Optional[str] = None
    if n_obs < 1 or tq < 1:
        note = "insufficient_observations_or_total_queries"
        return {
            "pricing_as_of": PRICING_AS_OF,
            "total_queries": tq,
            "observed_queries": n_obs,
            "avg_tokens_per_query": int(avg_tokens_per_query),
            "baseline_model": baseline_model,
            "cost_without_routing": 0.0,
            "cost_with_routing": 0.0,
            "savings_usd": 0.0,
            "savings_pct": 0.0,
            "cost_per_query_without_usd": 0.0,
            "cost_per_query_with_usd": 0.0,
            "route_fraction": {},
            "note": note,
        }

    avg = max(1, int(avg_tokens_per_query))
    dist = {
        k: float(routing_stats.get(k, 0) or 0) / float(n_obs)
        for k in ("fast", "verify", "rag", "escalate", "abstain")
    }
    c_base = estimate_cost(baseline_model, avg, pricing=p)
    c_mix = sum(
        dist[k] * estimate_cost(_TIER_MODEL_FOR_SAVINGS[k], avg, pricing=p) for k in dist
    )
    cost_without = float(tq) * c_base
    cost_with = float(tq) * c_mix
    savings = cost_without - cost_with
    denom = max(cost_without, 1e-12)
    return {
        "pricing_as_of": PRICING_AS_OF,
        "total_queries": tq,
        "observed_queries": n_obs,
        "avg_tokens_per_query": avg,
        "baseline_model": baseline_model,
        "cost_without_routing": cost_without,
        "cost_with_routing": cost_with,
        "savings_usd": savings,
        "savings_pct": savings / denom,
        "cost_per_query_without_usd": cost_without / float(tq),
        "cost_per_query_with_usd": cost_with / float(tq),
        "route_fraction": dist,
        "note": note,
    }


@dataclass
class RouteDecision:
    route: str
    sigma_pre: float
    sigma_post: Optional[float]
    response: str
    model_used: str
    tokens_used: int
    cost_estimate: float
    metadata: Dict[str, Any] = field(default_factory=dict)


class SigmaRouter:
    """
    Map ``sigma_pre`` (from ``SigmaPrecheck.predict``) to a tier using the **cognitive
    interrupt** in ``sigma_gate_core`` (Python mirror of ``sigma_gate.h``):

    * **ACCEPT** → FAST (local generation)
    * **RETHINK** + ``k_eff`` high → RAG when ``rag_fn`` is set, else VERIFY
    * **RETHINK** + ``k_eff`` low → ESCALATE when ``strong_model_fn`` is set, else VERIFY
    * **ABSTAIN** → ABSTAIN

    The ``post_gate`` (LSD) argument is **deprecated** and ignored for routing.

    When ``route_by_precheck_thresholds`` is **True**, tiers follow ``sigma_pre`` bands
    (``tau_fast`` / ``tau_verify`` / ``tau_escalate``) instead of the cognitive interrupt —
    useful when a downstream LSD **post**-gate would otherwise OOD-collapse every query to
    ABSTAIN during router-only harnesses.  Cognitive routing remains the default.

    Per-query **statistics** count only the **final** returned tier (mutually exclusive).
    """

    def __init__(
        self,
        *,
        precheck: Any = None,
        post_gate: Any = None,
        local_model: Any = None,
        local_tokenizer: Any = None,
        strong_model_fn: Optional[Callable[[str], str]] = None,
        rag_fn: Optional[Callable[[str], str]] = None,
        thresholds: Optional[Dict[str, float]] = None,
        pricing: Optional[Mapping[str, float]] = None,
        cost_assumed_api_tokens: int = 300,
        route_by_precheck_thresholds: bool = False,
        tau_fast: float = 0.3,
        tau_verify: float = 0.6,
        tau_escalate: float = 0.85,
    ):
        self.precheck = precheck
        self.post_gate = post_gate  # deprecated for routing; retained for API compatibility
        self.local_model = local_model
        self.local_tokenizer = local_tokenizer
        self.strong_model_fn = strong_model_fn
        self.rag_fn = rag_fn
        self._pricing: Optional[Mapping[str, float]] = pricing
        self._cost_assumed_api_tokens = max(1, int(cost_assumed_api_tokens))
        self.route_by_precheck_thresholds = bool(route_by_precheck_thresholds)
        self.tau_fast = float(tau_fast)
        self.tau_verify = float(tau_verify)
        self.tau_escalate = float(tau_escalate)
        if thresholds:
            warnings.warn(
                "thresholds= is ignored; use route_by_precheck_thresholds + tau_* "
                "or default cognitive routing (sigma_gate_core).",
                DeprecationWarning,
                stacklevel=2,
            )
        self.stats: Dict[str, Any] = {
            "total": 0,
            "fast": 0,
            "verify": 0,
            "rag": 0,
            "escalate": 0,
            "abstain": 0,
            "total_tokens": 0,
            "total_cost": 0.0,
        }

    def _sigma_pre(self, question: str) -> float:
        if self.precheck is None or self.local_model is None or self.local_tokenizer is None:
            return 0.4
        _should, sigma_pre, _det = self.precheck.predict(
            self.local_model, self.local_tokenizer, question
        )
        return float(max(0.0, min(1.0, sigma_pre)))

    def _generate_local(self, question: str, *, max_new_tokens: int = 200) -> tuple[str, int]:
        import torch

        if self.local_model is None or self.local_tokenizer is None:
            return "", 0
        if self.local_tokenizer.pad_token is None:
            self.local_tokenizer.pad_token = self.local_tokenizer.eos_token
        inputs = self.local_tokenizer(question, return_tensors="pt")
        dev = next(self.local_model.parameters()).device
        inputs = {k: v.to(dev) for k, v in inputs.items()}
        in_len = int(inputs["input_ids"].shape[1])
        with torch.no_grad():
            out = self.local_model.generate(
                **inputs,
                max_new_tokens=int(max_new_tokens),
                do_sample=False,
                pad_token_id=self.local_tokenizer.pad_token_id,
            )
        response = self.local_tokenizer.decode(
            out[0, in_len:],
            skip_special_tokens=True,
        ).strip()
        new_toks = int(out.shape[1] - in_len)
        return response, new_toks

    def _p(self) -> Mapping[str, float]:
        return self._pricing if self._pricing is not None else load_token_pricing_usd_per_1k()

    def _route_from_cognitive(
        self,
        question: str,
        sigma_pre: float,
        *,
        max_new_tokens: int,
    ) -> RouteDecision:
        st = SigmaState()
        sp = float(max(0.0, min(1.0, sigma_pre)))
        k_raw = max(0.0, min(1.0, 1.0 - sp))
        sigma_update(st, sp, k_raw)
        v = sigma_gate(st)
        meta = {
            "cognitive_verdict": v.name,
            "k_eff_q16": st.k_eff,
            "d_sigma_q16": st.d_sigma,
            "sigma_pre_q16": st.sigma,
        }
        if v == Verdict.ABSTAIN:
            return self._route_abstain(question, sp, extra=meta)
        if v == Verdict.ACCEPT:
            d = self._route_fast(question, sp, max_new_tokens=max_new_tokens)
            d.metadata = {**d.metadata, **meta}
            return d
        if st.k_eff >= SIGMA_HALF and self.rag_fn is not None:
            d = self._route_rag(question, sp, max_new_tokens=max_new_tokens)
        elif st.k_eff < SIGMA_HALF and self.strong_model_fn is not None:
            d = self._route_escalate(question, sp, max_new_tokens=max_new_tokens)
        else:
            d = self._route_verify(question, sp, max_new_tokens=max_new_tokens)
        d.metadata = {**d.metadata, **meta}
        return d

    def _record_final(self, d: RouteDecision) -> None:
        self.stats["total"] += 1
        key = {
            "FAST": "fast",
            "VERIFY": "verify",
            "RAG": "rag",
            "ESCALATE": "escalate",
            "ABSTAIN": "abstain",
        }.get(d.route, "abstain")
        self.stats[key] += 1
        self.stats["total_tokens"] += int(d.tokens_used)
        self.stats["total_cost"] += float(d.cost_estimate)

    def route(self, question: str, *, max_new_tokens: int = 200) -> RouteDecision:
        sp = self._sigma_pre(question)
        if self.route_by_precheck_thresholds:
            d = self._route_from_precheck_bands(question, sp, max_new_tokens=max_new_tokens)
        else:
            d = self._route_from_cognitive(question, sp, max_new_tokens=max_new_tokens)
        self._record_final(d)
        return d

    def _route_from_precheck_bands(
        self,
        question: str,
        sigma_pre: float,
        *,
        max_new_tokens: int,
    ) -> RouteDecision:
        """Tier by ``sigma_pre`` vs ``tau_*`` without applying ``sigma_gate`` to the scalar."""
        sp = float(max(0.0, min(1.0, float(sigma_pre))))
        meta: Dict[str, Any] = {
            "routing_mode": "precheck_sigma_bands",
            "sigma_pre": sp,
            "tau_fast": self.tau_fast,
            "tau_verify": self.tau_verify,
            "tau_escalate": self.tau_escalate,
        }
        if sp <= self.tau_fast:
            d = self._route_fast(question, sp, max_new_tokens=max_new_tokens)
        elif sp <= self.tau_verify:
            d = self._route_verify(question, sp, max_new_tokens=max_new_tokens)
        elif sp <= self.tau_escalate:
            if self.rag_fn is not None:
                d = self._route_rag(question, sp, max_new_tokens=max_new_tokens)
            else:
                d = self._route_verify(question, sp, max_new_tokens=max_new_tokens)
        elif self.strong_model_fn is not None:
            d = self._route_escalate(question, sp, max_new_tokens=max_new_tokens)
        else:
            d = self._route_abstain(
                question,
                sp,
                extra={"reason": "sigma_above_escalate_band_no_strong_model_fn"},
            )
        d.metadata = {**d.metadata, **meta}
        return d

    def _route_fast(self, question: str, sigma_pre: float, *, max_new_tokens: int) -> RouteDecision:
        response, toks = self._generate_local(question, max_new_tokens=max_new_tokens)
        return RouteDecision(
            route="FAST",
            sigma_pre=sigma_pre,
            sigma_post=None,
            response=response,
            model_used="local",
            tokens_used=toks,
            cost_estimate=estimate_cost("local_hosted", max(toks, 1), pricing=self._p()),
            metadata={"tier": "fast", "note": "sigma_gate_core cognitive routing"},
        )

    def _route_verify(self, question: str, sigma_pre: float, *, max_new_tokens: int) -> RouteDecision:
        response, toks = self._generate_local(question, max_new_tokens=max_new_tokens)
        return RouteDecision(
            route="VERIFY",
            sigma_pre=sigma_pre,
            sigma_post=None,
            response=response,
            model_used="local",
            tokens_used=toks,
            cost_estimate=estimate_cost("local_hosted", max(toks, 1), pricing=self._p()),
            metadata={
                "tier": "verify",
                "note": "precheck_only_routing_v56; post_gate ignored",
            },
        )

    def _route_rag(self, question: str, sigma_pre: float, *, max_new_tokens: int) -> RouteDecision:
        if self.rag_fn is not None:
            try:
                response = self.rag_fn(question)
            except Exception as e:
                return RouteDecision(
                    route="RAG",
                    sigma_pre=sigma_pre,
                    sigma_post=None,
                    response="",
                    model_used="rag_error",
                    tokens_used=0,
                    cost_estimate=0.0,
                    metadata={"error": str(e)},
                )
            api_toks = self._cost_assumed_api_tokens
            return RouteDecision(
                route="RAG",
                sigma_pre=sigma_pre,
                sigma_post=None,
                response=response,
                model_used="local+RAG",
                tokens_used=0,
                cost_estimate=estimate_cost("gpt-4o-mini", api_toks, pricing=self._p()),
                metadata={"tier": "rag", "assumed_api_tokens": api_toks},
            )
        return self._route_escalate(question, sigma_pre, max_new_tokens=max_new_tokens)

    def _route_escalate(
        self,
        question: str,
        sigma_pre: float,
        *,
        max_new_tokens: int,
        prior_local_tokens: int = 0,
    ) -> RouteDecision:
        del max_new_tokens
        if self.strong_model_fn is not None:
            try:
                response = self.strong_model_fn(question)
            except Exception as e:
                return self._route_abstain(
                    question,
                    sigma_pre,
                    extra={"escalate_error": str(e)},
                    prior_local_tokens=prior_local_tokens,
                )
            api_toks = self._cost_assumed_api_tokens
            local_part = estimate_cost(
                "local_hosted", max(int(prior_local_tokens), 0), pricing=self._p()
            )
            api_part = estimate_cost("gpt-4o", api_toks, pricing=self._p())
            return RouteDecision(
                route="ESCALATE",
                sigma_pre=sigma_pre,
                sigma_post=None,
                response=response,
                model_used="strong_api",
                tokens_used=int(prior_local_tokens),
                cost_estimate=float(local_part + api_part),
                metadata={"tier": "escalate", "assumed_api_tokens": api_toks},
            )
        return self._route_abstain(
            question,
            sigma_pre,
            extra={"reason": "no_strong_model_fn"},
            prior_local_tokens=prior_local_tokens,
        )

    def _route_abstain(
        self,
        question: str,
        sigma_pre: float,
        *,
        extra: Optional[Dict[str, Any]] = None,
        prior_local_tokens: int = 0,
    ) -> RouteDecision:
        msg = (
            "I do not have enough calibrated confidence to answer this accurately "
            "within the current routing policy."
        )
        meta: Dict[str, Any] = {"reason": "sigma_pre_tier_or_escalation_fallback", "question_len": len(question)}
        if extra:
            meta.update(extra)
        abstain_cost = estimate_cost(
            "local_hosted", max(int(prior_local_tokens), 0), pricing=self._p()
        )
        return RouteDecision(
            route="ABSTAIN",
            sigma_pre=sigma_pre,
            sigma_post=1.0,
            response=msg,
            model_used="none",
            tokens_used=int(prior_local_tokens),
            cost_estimate=float(abstain_cost),
            metadata=meta,
        )

    def get_stats(self) -> Dict[str, Any]:
        total = max(int(self.stats["total"]), 1)
        return {
            **dict(self.stats),
            "fast_pct": self.stats["fast"] / total,
            "verify_pct": self.stats["verify"] / total,
            "rag_pct": self.stats["rag"] / total,
            "escalate_pct": self.stats["escalate"] / total,
            "abstain_pct": self.stats["abstain"] / total,
            "avg_cost": float(self.stats["total_cost"]) / total,
        }


__all__ = [
    "PRICING_AS_OF",
    "SigmaRouter",
    "RouteDecision",
    "calculate_savings",
    "default_token_pricing_usd_per_1k",
    "estimate_cost",
    "load_token_pricing_usd_per_1k",
]
