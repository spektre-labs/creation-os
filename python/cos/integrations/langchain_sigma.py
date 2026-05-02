# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""LangChain σ-gate callback (optional ``langchain-core``).

One-line wiring::

    chain.invoke(input, config={"callbacks": [SigmaGateCallback(gate)]})

Install::

    pip install 'creation-os[langchain]'
"""
from __future__ import annotations

import logging
import warnings
from typing import Any, Dict, List, Optional

from cos.exceptions import SigmaAbstainError
from cos.sigma_gate import SigmaGate

try:
    from langchain_core.callbacks import BaseCallbackHandler
except ImportError:  # pragma: no cover
    try:
        from langchain.callbacks.base import BaseCallbackHandler  # type: ignore
    except ImportError:
        BaseCallbackHandler = None  # type: ignore[misc, assignment]

_LOG = logging.getLogger(__name__)


def _llm_output_text(response: Any) -> str:
    try:
        gen0 = response.generations[0][0]
        t = getattr(gen0, "text", None)
        if t is not None:
            return str(t)
        msg = getattr(gen0, "message", None)
        if msg is not None:
            c = getattr(msg, "content", None)
            if c is not None:
                return str(c)
    except (AttributeError, IndexError, TypeError):
        pass
    return ""


def _message_list_prompt(messages: Any) -> str:
    """Best-effort prompt string from chat-model ``messages`` batches."""
    try:
        if not messages:
            return ""
        batch = messages[0] if isinstance(messages[0], (list, tuple)) else messages
        if not batch:
            return ""
        last = batch[-1]
        c = getattr(last, "content", last)
        return str(c) if c is not None else ""
    except (TypeError, IndexError, AttributeError):
        return ""


if BaseCallbackHandler is not None:

    class SigmaGateCallback(BaseCallbackHandler):  # type: ignore[no-redef]
        """Scores each LLM completion; tracks ``run_id`` ↔ prompt; records ``traces``."""

        def __init__(
            self,
            gate: Any = None,
            *,
            on_abstain: str = "warn",
            tau_hi: float = 0.7,
            tau_lo: float = 0.3,
        ) -> None:
            super().__init__()
            if on_abstain not in ("raise", "warn", "log"):
                raise ValueError("on_abstain must be 'raise', 'warn', or 'log'")
            self.gate = gate or SigmaGate(threshold_accept=tau_lo, threshold_abstain=tau_hi)
            self.on_abstain = str(on_abstain)
            self.tau_hi = float(tau_hi)
            self.tau_lo = float(tau_lo)
            self.traces: List[Dict[str, Any]] = []
            self._current_prompts: Dict[str, str] = {}

        @property
        def ignore_llm(self) -> bool:
            return False

        @property
        def ignore_chain(self) -> bool:
            return True

        @property
        def ignore_agent(self) -> bool:
            return True

        @property
        def history(self) -> List[Dict[str, Any]]:
            """Alias for compatibility with older lab code."""
            return self.traces

        def _run_id_key(self, kwargs: Any) -> Optional[str]:
            rid = kwargs.get("run_id")
            return str(rid) if rid is not None else None

        def on_llm_start(
            self,
            serialized: Dict[str, Any],
            prompts: List[str],
            **kwargs: Any,
        ) -> None:
            key = self._run_id_key(kwargs)
            if key and prompts:
                self._current_prompts[key] = str(prompts[0])

        def on_llm_new_token(self, *args: Any, **kwargs: Any) -> None:
            del args, kwargs

        def on_chat_model_start(
            self,
            serialized: Dict[str, Any],
            messages: List[Any],
            **kwargs: Any,
        ) -> None:
            key = self._run_id_key(kwargs)
            if key:
                self._current_prompts[key] = _message_list_prompt(messages)

        def on_llm_end(self, response: Any, **kwargs: Any) -> None:
            text = _llm_output_text(response)
            if not text.strip():
                return
            key = self._run_id_key(kwargs)
            prompt = self._current_prompts.pop(key, "") if key else ""
            if not prompt:
                prompt = _first_prompt_kwargs_fallback(kwargs)

            sigma, verdict = self.gate.score(prompt, text)

            trace = {
                "run_id": key or "",
                "sigma": sigma,
                "verdict": verdict,
                "prompt": prompt[:100],
                "response": text[:100],
            }
            self.traces.append(trace)

            if verdict == "ABSTAIN":
                msg = f"σ-gate ABSTAIN: σ={sigma:.3f}"
                if self.on_abstain == "raise":
                    raise SigmaAbstainError(msg)
                if self.on_abstain == "warn":
                    warnings.warn(msg, stacklevel=1)
                else:
                    _LOG.warning(msg)

            if verdict != "ABSTAIN" or self.on_abstain != "raise":
                try:
                    gen0 = response.generations[0][0]
                    meta = getattr(gen0, "generation_info", None)
                    merged = dict(meta) if isinstance(meta, dict) else {}
                    merged["sigma"] = sigma
                    merged["verdict"] = verdict
                    gen0.generation_info = merged  # type: ignore[misc]
                except (AttributeError, IndexError, TypeError):
                    pass

        def on_llm_error(self, error: BaseException, **kwargs: Any) -> None:
            del error
            key = self._run_id_key(kwargs)
            if key and key in self._current_prompts:
                self._current_prompts.pop(key, None)

else:  # pragma: no cover

    class SigmaGateCallback:  # type: ignore[no-redef]
        def __init__(self, *args: Any, **kwargs: Any) -> None:
            raise ImportError(
                "LangChain callbacks require langchain-core. "
                "Install with: pip install 'creation-os[langchain]'"
            )


def _first_prompt_kwargs_fallback(kwargs: Any) -> str:
    p = kwargs.get("prompts")
    if isinstance(p, list) and p:
        return str(p[0])
    return ""


# v152 alias (user-facing name)
SigmaCallback = SigmaGateCallback


__all__ = [
    "SigmaGateCallback",
    "SigmaCallback",
    "SigmaAbstainError",
]
