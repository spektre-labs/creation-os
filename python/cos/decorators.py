# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
"""``@sigma_gated`` — optional σ check on any callables that return model text."""
from __future__ import annotations

from functools import wraps
from typing import Any, Callable, Optional, TypeVar, cast, overload

from cos.exceptions import SigmaAbstainError
from cos.scoring import default_scorer

F = TypeVar("F", bound=Callable[..., Any])


@overload
def sigma_gated(func: F) -> F: ...


@overload
def sigma_gated(
    func: None = None,
    *,
    gate: Optional[Any] = None,
    on_abstain: str = "raise",
) -> Callable[[F], F]: ...


def sigma_gated(
    func: Optional[F] = None,
    *,
    gate: Optional[Any] = None,
    on_abstain: str = "raise",
) -> Any:
    """Wrap ``func``; after execution, score ``str(args[0])`` vs ``str(result)``.

    * ``on_abstain="raise"`` → :exc:`SigmaAbstainError`
    * ``on_abstain="return_none"`` → return ``None``
    """

    def decorator(f: F) -> F:
        scorer = default_scorer(gate)

        @wraps(f)
        def wrapper(*args: Any, **kwargs: Any):
            result = f(*args, **kwargs)
            prompt = str(args[0]) if args else ""
            sigma, verdict = scorer.score(prompt, str(result))
            if verdict == "ABSTAIN":
                if on_abstain == "raise":
                    raise SigmaAbstainError(f"σ-gate ABSTAIN (σ={sigma:.3f})")
                if on_abstain == "return_none":
                    return None
                raise SigmaAbstainError(f"unknown on_abstain={on_abstain!r}")
            return result

        return cast(F, wrapper)

    if func is not None:
        return decorator(func)
    return decorator


__all__ = ["sigma_gated", "SigmaAbstainError"]
