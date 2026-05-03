# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-compose — functional pipeline builder (σ propagation, branch, retry, circuit breaker).

YAML loading uses PyYAML when installed; otherwise the file must be **JSON** (``compose.json``
or ``.yaml`` with JSON content). See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Callable, Dict, List, Mapping, Union

__all__ = ["SigmaCompose"]

Step = Callable[[Dict[str, Any]], Dict[str, Any]]


class SigmaCompose:
    """Composable steps: each step receives and returns a **context** dict."""

    @staticmethod
    def sigma_propagation(ctx: Mapping[str, Any]) -> float:
        """Max σ seen along the pipeline (conservative stress)."""
        return float(max(ctx.get("_sigmas") or [float(ctx.get("sigma", 0.0))]))

    @staticmethod
    def pipeline(*steps: Step) -> Step:
        def run(ctx: Dict[str, Any]) -> Dict[str, Any]:
            c = dict(ctx)
            sigmas: List[float] = list(c.get("_sigmas", []))
            for st in steps:
                c = dict(st(c))
                if "sigma" in c:
                    sigmas.append(float(c["sigma"]))
                c["_sigmas"] = sigmas
                c["_sigma_max"] = max(sigmas) if sigmas else float(c.get("sigma", 0.0))
            return c

        return run

    @staticmethod
    def branch(condition: Callable[[Dict[str, Any]], bool], if_true: Step, if_false: Step) -> Step:
        def run(ctx: Dict[str, Any]) -> Dict[str, Any]:
            return if_true(ctx) if condition(ctx) else if_false(ctx)

        return run

    @staticmethod
    def parallel(*steps: Step) -> Step:
        """Run steps on copied ctx; merge **minimum** σ (optimistic fuse)."""

        def run(ctx: Dict[str, Any]) -> Dict[str, Any]:
            outs: List[Dict[str, Any]] = []
            for st in steps:
                outs.append(dict(st(dict(ctx))))
            sigmas = [float(o.get("sigma", 0.5)) for o in outs]
            best = min(sigmas) if sigmas else 0.5
            merged = dict(ctx)
            merged["sigma"] = best
            merged["_parallel_branches"] = outs
            merged["_sigmas"] = sigmas
            return merged

        return run

    @staticmethod
    def retry(step: Step, max_retries: int, *, on: str = "RETHINK") -> Step:
        target = str(on).upper()

        def run(ctx: Dict[str, Any]) -> Dict[str, Any]:
            last = dict(ctx)
            for attempt in range(1, int(max_retries) + 2):
                last = dict(step(last))
                last["attempt"] = attempt
                if str(last.get("verdict", "")).upper() != target:
                    break
            return last

        return run

    @staticmethod
    def circuit_breaker(step: Step, max_fails: int) -> Step:
        fails = {"n": 0}

        def run(ctx: Dict[str, Any]) -> Dict[str, Any]:
            if fails["n"] >= int(max_fails):
                out = dict(ctx)
                out["circuit_open"] = True
                out["verdict"] = "ABSTAIN"
                out["sigma"] = 1.0
                return out
            out = dict(step(dict(ctx)))
            if str(out.get("verdict", "")).upper() == "ABSTAIN":
                fails["n"] += 1
            else:
                fails["n"] = 0
            return out

        return run

    @classmethod
    def from_yaml(cls, path: Union[str, Path]) -> Step:
        """Load a minimal recipe: ``{"steps": ["retrieve", "gate"]}`` dispatches named stubs."""
        p = Path(path)
        raw = p.read_text(encoding="utf-8")
        try:
            import yaml  # type: ignore

            data = yaml.safe_load(raw)
        except ImportError:
            data = json.loads(raw)
        if not isinstance(data, Mapping):
            raise ValueError("compose file must be a mapping")
        names = list(data.get("steps") or [])
        stubs: Dict[str, Step] = {
            "retrieve": lambda c: {**c, "sigma": float(c.get("sigma", 0.1))},
            "rerank": lambda c: {**c, "sigma": max(float(c.get("sigma", 0)), 0.15)},
            "generate": lambda c: {**c, "sigma": max(float(c.get("sigma", 0)), 0.2)},
            "gate": lambda c: {**c, "verdict": "ACCEPT", "sigma": max(float(c.get("sigma", 0)), 0.25)},
            "explain": lambda c: {**c, "explained": True},
        }
        resolved = [stubs[str(n)] for n in names]
        return cls.pipeline(*resolved) if resolved else (lambda c: dict(c))
