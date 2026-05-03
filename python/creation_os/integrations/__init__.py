# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""σ-gate framework shims (requires the ``creation-os`` PyPI package)."""
from __future__ import annotations

from typing import Any

__all__ = [
    "SigmaAbstainError",
    "SigmaAutoGenHook",
    "SigmaCallback",
    "SigmaGateCallback",
    "SigmaGateHook",
    "SigmaGateTool",
    "sigma_gate_node",
    "sigma_gate_router",
    "sigma_gated",
    "sigma_gated_llm",
]


def __getattr__(name: str) -> Any:
    try:
        import cos  # noqa: F401
    except ImportError as e:  # pragma: no cover
        raise ImportError(
            "creation_os.integrations requires the Creation OS runtime. "
            "Install with: pip install creation-os"
        ) from e
    if name == "SigmaAbstainError":
        from cos.exceptions import SigmaAbstainError

        return SigmaAbstainError
    if name == "SigmaGateCallback":
        from cos.integrations.langchain import SigmaGateCallback

        return SigmaGateCallback
    if name == "SigmaCallback":
        from cos.integrations.langchain_sigma import SigmaCallback

        return SigmaCallback
    if name in ("sigma_gate_node", "sigma_gate_router"):
        from cos.integrations import langgraph_sigma

        return getattr(langgraph_sigma, name)
    if name == "SigmaGateTool":
        from cos.integrations.crewai_sigma import SigmaGateTool

        return SigmaGateTool
    if name == "SigmaAutoGenHook":
        from cos.integrations.autogen_sigma import SigmaAutoGenHook

        return SigmaAutoGenHook
    if name == "SigmaGateHook":
        from cos.integrations.autogen_sigma import SigmaGateHook

        return SigmaGateHook
    if name == "sigma_gated":
        from cos.decorators import sigma_gated

        return sigma_gated
    if name == "sigma_gated_llm":
        from cos.integrations.decorator import sigma_gated_llm

        return sigma_gated_llm
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
