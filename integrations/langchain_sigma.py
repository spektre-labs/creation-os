"""σ-gate helpers for LangChain apps (optional LangChain dependency).

Install: ``pip install requests`` and either ``pip install -e sdk/python`` or set
``PYTHONPATH`` to the repo ``sdk/python`` directory.  For callbacks, also
``pip install langchain-core`` (or ``langchain``).

``SigmaGateClient`` exposes ``check`` and ``gate_or_raise``.  ``SigmaGateCallback``
delegates to ``gate_or_raise`` on each ``on_llm_end`` when LangChain is installed.
"""

from __future__ import annotations

import os
from typing import Any, Dict, Optional, Tuple

from creation_os import CreationOS

try:
    from langchain_core.callbacks import BaseCallbackHandler
except ImportError:  # pragma: no cover
    try:
        from langchain.callbacks.base import BaseCallbackHandler
    except ImportError:
        BaseCallbackHandler = None  # type: ignore[misc, assignment]


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


class SigmaGateClient:
    """``check`` / ``gate_or_raise`` around ``CreationOS.verify``."""

    def __init__(self, host: str = "localhost", port: Optional[int] = None, tau: float = 0.6) -> None:
        p = port if port is not None else int(os.environ.get("COS_SERVE_PORT", "3001"))
        self.cos = CreationOS(host=host, port=p)
        self.tau = tau

    def check(self, text: str, model: str = "gemma3:4b") -> Tuple[Optional[str], Dict[str, Any]]:
        result = self.cos.verify(text, model=model)
        if result.get("action") == "ABSTAIN":
            return None, result
        return text, result

    def gate_or_raise(self, text: str, model: str = "gemma3:4b") -> Tuple[str, Dict[str, Any]]:
        result = self.cos.verify(text, model=model)
        if result.get("action") == "ABSTAIN":
            sig = float(result.get("sigma", 0.0))
            raise ValueError(
                f"sigma-gate ABSTAIN (sigma={sig:.3f}): output blocked for safety; "
                f"audit_id={result.get('audit_id')}"
            )
        return text, result


if BaseCallbackHandler is not None:

    class SigmaGateCallback(BaseCallbackHandler):  # type: ignore[no-redef]
        """``callbacks=[SigmaGateCallback()]`` — verify each completion; raise on ABSTAIN."""

        def __init__(self, host: str = "localhost", port: Optional[int] = None, tau: float = 0.6) -> None:
            super().__init__()
            self._sg = SigmaGateClient(host=host, port=port, tau=tau)

        def check(self, text: str, model: str = "gemma3:4b") -> Tuple[Optional[str], Dict[str, Any]]:
            return self._sg.check(text, model=model)

        def gate_or_raise(self, text: str, model: str = "gemma3:4b") -> Tuple[str, Dict[str, Any]]:
            return self._sg.gate_or_raise(text, model=model)

        def on_llm_end(self, response: Any, **kwargs: Any) -> None:
            text = _llm_output_text(response)
            if not text.strip():
                return
            self.gate_or_raise(text)

else:
    SigmaGateCallback = SigmaGateClient  # type: ignore[misc, assignment, no-redef]
