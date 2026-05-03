# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-cost — lab scaffolding for **cost-aware model routing** and **per-verdict billing** sketches.

Uses gate :meth:`score` / :meth:`__call__` outputs (ACCEPT / RETHINK / ABSTAIN). Does not modify
``sigma_gate.h``. Not a financial or contractual billing system; operators validate pricing tables.
"""
from __future__ import annotations

import calendar
import json
import time
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, DefaultDict, Dict, List, Optional, Sequence, Tuple

GenerateFn = Callable[[str, str], str]


def _default_usage_record() -> Dict[str, Any]:
    return {
        "input_tokens": 0.0,
        "output_tokens": 0.0,
        "cost_usd": 0.0,
        "queries": 0,
        "verdicts": {"ACCEPT": 0, "RETHINK": 0, "ABSTAIN": 0},
    }


def _normalize_verdict(v: Any) -> str:
    s = str(v).strip().upper()
    if "." in s:
        s = s.rsplit(".", 1)[-1]
    if s in ("ACCEPT", "RETHINK", "ABSTAIN"):
        return s
    return "ABSTAIN" if "ABSTAIN" in s else "RETHINK" if "RETHINK" in s else "ACCEPT" if "ACCEPT" in s else s


def _token_estimate(text: str) -> float:
    """Very rough token proxy (lab); replace with real tokenizer counts in harness."""
    return max(1.0, len(text.split()) * 1.3)


class SigmaCost:
    MODEL_PRICING: Dict[str, Dict[str, Any]] = {
        "bitnet-2b": {"input": 0.0, "output": 0.0, "local": True},
        "gemma-2b": {"input": 0.0, "output": 0.0, "local": True},
        "llama-8b": {"input": 0.03, "output": 0.06, "local": False},
        "llama-70b": {"input": 0.50, "output": 1.50, "local": False},
        "gpt-4o": {"input": 2.50, "output": 10.00, "local": False},
    }

    def __init__(
        self,
        gate: Any,
        *,
        generate: Optional[GenerateFn] = None,
    ) -> None:
        self.gate = gate
        self.generate_fn: Optional[GenerateFn] = generate
        self.usage: DefaultDict[str, Dict[str, Any]] = defaultdict(_default_usage_record)
        self.saved_routing_usd: float = 0.0
        self.saved_abstain_usd: float = 0.0

    def set_generate(self, fn: GenerateFn) -> None:
        self.generate_fn = fn

    def generate(self, model_name: str, prompt: str) -> str:
        if self.generate_fn is None:
            raise RuntimeError("SigmaCost.generate_fn not set; pass generate= to __init__ or call set_generate()")
        return self.generate_fn(model_name, prompt)

    def gate_score(self, prompt: str, response: str) -> Tuple[float, str]:
        score = getattr(self.gate, "score", None)
        if callable(score):
            sigma, verdict = score(prompt, response)
            return float(sigma), _normalize_verdict(verdict)
        call = getattr(self.gate, "__call__", None)
        if callable(call):
            sigma, verdict = call(None, None, prompt, response)
            return float(sigma), _normalize_verdict(verdict)
        raise TypeError("gate must expose score() or __call__(model, tok, prompt, response)")

    def compute_cost(self, model: str, input_tokens: float, output_tokens: float) -> float:
        pricing = self.MODEL_PRICING.get(model, {"input": 0.0, "output": 0.0})
        inp = float(pricing.get("input", 0.0))
        out = float(pricing.get("output", 0.0))
        return (float(input_tokens) * inp + float(output_tokens) * out) / 1_000_000.0

    def track(
        self,
        model: str,
        input_tokens: float,
        output_tokens: float,
        cost: float,
        verdict: str,
        *,
        bill_cost: bool = True,
    ) -> None:
        u = self.usage[model]
        u["input_tokens"] += float(input_tokens)
        u["output_tokens"] += float(output_tokens)
        if bill_cost:
            u["cost_usd"] += float(cost)
        u["queries"] = int(u["queries"]) + 1
        v = _normalize_verdict(verdict)
        vd = u["verdicts"]
        if v not in vd:
            vd[v] = 0
        vd[v] += 1

    def compare_route_cost(self, reference_model: str, input_tokens: float, output_tokens: float) -> float:
        """USD for hypothetical generation on ``reference_model`` (used for routing savings)."""
        return self.compute_cost(reference_model, input_tokens, output_tokens)

    def route_by_cost(
        self,
        prompt: str,
        models: Optional[Sequence[str]] = None,
    ) -> Dict[str, Any]:
        chain = list(models or ["bitnet-2b", "llama-8b", "llama-70b"])
        if not chain:
            return {"error": "empty models list"}

        reference = chain[-1]
        last: Dict[str, Any] = {
            "response": "",
            "model": reference,
            "sigma": 1.0,
            "verdict": "ABSTAIN",
            "cost_usd": 0.0,
        }

        for model_name in chain:
            response = self.generate(model_name, prompt)
            sigma, verdict = self.gate_score(prompt, response)
            input_tokens = _token_estimate(prompt)
            output_tokens = _token_estimate(response)
            cost = self.compute_cost(model_name, input_tokens, output_tokens)

            if verdict == "ABSTAIN":
                self.track(model_name, input_tokens, output_tokens, cost, verdict, bill_cost=False)
                self.saved_abstain_usd += float(cost)
                return {
                    "response": None,
                    "model": model_name,
                    "sigma": sigma,
                    "verdict": "ABSTAIN",
                    "cost_usd": 0.0,
                    "reason": "not reliable",
                    "saved_estimate_usd": float(cost),
                }

            self.track(model_name, input_tokens, output_tokens, cost, verdict, bill_cost=True)

            if verdict == "ACCEPT":
                spend = self.compare_route_cost(reference, input_tokens, output_tokens)
                savings = max(0.0, float(spend) - float(cost))
                self.saved_routing_usd += savings
                return {
                    "response": response,
                    "model": model_name,
                    "sigma": sigma,
                    "verdict": verdict,
                    "cost_usd": float(cost),
                    "savings_usd": savings,
                }

            last = {
                "response": response,
                "model": model_name,
                "sigma": sigma,
                "verdict": verdict,
                "cost_usd": float(cost),
            }

        return {
            "response": last.get("response"),
            "model": last.get("model"),
            "sigma": last.get("sigma"),
            "verdict": last.get("verdict"),
            "cost_usd": float(last.get("cost_usd", 0.0)),
            "note": "exhausted_model_chain",
        }

    def report(self) -> Dict[str, Any]:
        per_model = {k: dict(v) for k, v in self.usage.items()}
        total_tracked = sum(float(u["cost_usd"]) for u in per_model.values())
        denom = max(
            total_tracked + self.saved_routing_usd + self.saved_abstain_usd,
            1e-9,
        )
        savings_pct = round((self.saved_routing_usd + self.saved_abstain_usd) / denom * 100.0, 1)
        return {
            "total_cost_usd": round(total_tracked, 4),
            "total_saved_routing_usd": round(self.saved_routing_usd, 4),
            "total_saved_abstain_usd": round(self.saved_abstain_usd, 4),
            "savings_percent": savings_pct,
            "per_model": per_model,
            "per_verdict_pricing_ready": True,
        }


class SigmaBillingMeter:
    """Per-verdict metering sketch: only ACCEPT rows are billable at ``price_per_verdict``."""

    def __init__(self, price_per_verdict: float = 0.001) -> None:
        self.price = float(price_per_verdict)
        self.events: List[Dict[str, Any]] = []

    def record(self, user_id: str, sigma: float, verdict: Any, model: str) -> Dict[str, Any]:
        v = _normalize_verdict(verdict)
        event = {
            "user_id": user_id,
            "sigma": float(sigma),
            "verdict": v,
            "model": model,
            "billable": v == "ACCEPT",
            "amount": float(self.price) if v == "ACCEPT" else 0.0,
            "timestamp": time.time(),
        }
        self.events.append(event)
        return event

    def invoice(self, user_id: str, period: str) -> Dict[str, Any]:
        """``period`` is ``YYYY-MM`` (UTC month boundaries)."""
        t0, t1 = _month_range_utc(period.strip())
        user_events = [
            e
            for e in self.events
            if e.get("user_id") == user_id and t0 <= float(e.get("timestamp", 0)) <= t1 and e.get("billable")
        ]
        total_amt = sum(float(e.get("amount", 0.0)) for e in user_events)
        return {
            "user_id": user_id,
            "period": period,
            "total_verdicts": len(user_events),
            "total_amount": round(total_amt, 6),
            "price_per_verdict": self.price,
        }


class LabCostGate:
    """Deterministic gate for demos: short factual answers → ACCEPT, hedge words → RETHINK, junk → ABSTAIN."""

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        p, r = prompt.lower(), response.lower()
        if any(x in r for x in ("i don't know", "cannot say", "nonsense", "@@@")):
            return 0.92, "ABSTAIN"
        if "2+2" in p or "2 + 2" in p:
            if any(tok in r for tok in ("4", "four")) and "maybe" not in r and "or " not in r:
                return 0.04, "ACCEPT"
            return 0.55, "RETHINK"
        if "maybe" in r or "probably" in r or "unclear" in r:
            return 0.55, "RETHINK"
        if len(r.split()) <= 3 and r.strip():
            return 0.12, "ACCEPT"
        return 0.45, "RETHINK"


def _month_range_utc(ym: str) -> Tuple[float, float]:
    parts = ym.split("-")
    if len(parts) != 2:
        raise ValueError(f"period must be YYYY-MM, got {ym!r}")
    year, month = int(parts[0]), int(parts[1])
    start = datetime(year, month, 1, tzinfo=timezone.utc)
    _, last_day = calendar.monthrange(year, month)
    end = datetime(year, month, last_day, 23, 59, 59, tzinfo=timezone.utc)
    return start.timestamp(), end.timestamp()


def default_cost_state_path() -> Path:
    home = Path.home()
    d = home / ".cos"
    d.mkdir(parents=True, exist_ok=True)
    return d / "sigma_cost_lab.json"


def load_lab_state(path: Path) -> Tuple[SigmaCost, SigmaBillingMeter]:
    if path.is_file():
        blob = json.loads(path.read_text(encoding="utf-8"))
    else:
        blob = {}

    gate = LabCostGate()
    cost = SigmaCost(gate)
    meter = SigmaBillingMeter(float(blob.get("price_per_verdict", 0.001)))

    usage_blob = blob.get("usage") or {}
    if isinstance(usage_blob, dict):
        for k, v in usage_blob.items():
            if isinstance(v, dict):
                cost.usage[k] = {**_default_usage_record(), **v}
    cost.saved_routing_usd = float(blob.get("saved_routing_usd", 0.0))
    cost.saved_abstain_usd = float(blob.get("saved_abstain_usd", 0.0))

    events = blob.get("billing_events") or []
    if isinstance(events, list):
        meter.events = [e for e in events if isinstance(e, dict)]

    return cost, meter


def save_lab_state(
    path: Path,
    cost: SigmaCost,
    meter: SigmaBillingMeter,
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    out = {
        "usage": {k: dict(v) for k, v in cost.usage.items()},
        "saved_routing_usd": cost.saved_routing_usd,
        "saved_abstain_usd": cost.saved_abstain_usd,
        "billing_events": list(meter.events),
        "price_per_verdict": meter.price,
    }
    path.write_text(json.dumps(out, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


__all__ = [
    "SigmaCost",
    "SigmaBillingMeter",
    "LabCostGate",
    "load_lab_state",
    "save_lab_state",
    "default_cost_state_path",
]
