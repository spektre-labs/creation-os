"""Optional LangChain callback: verify each LLM completion via cos-serve.

Install: pip install langchain requests  (or langchain-core where applicable)
Set COS_SERVE_PORT if not using 3001.

Add `sdk/python` and `integrations` parent to PYTHONPATH, or run from repo with:
  PYTHONPATH=sdk/python:integrations python your_app.py
"""

from __future__ import annotations

import os
import sys
from typing import Any, Optional

try:
    from langchain_core.callbacks import BaseCallbackHandler
except ImportError:  # pragma: no cover
    try:
        from langchain.callbacks.base import BaseCallbackHandler
    except ImportError:
        BaseCallbackHandler = None  # type: ignore[misc, assignment]

_SDK = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "sdk", "python"))
if _SDK not in sys.path:
    sys.path.insert(0, _SDK)

from creation_os import CreationOS  # noqa: E402


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


def _missing_langchain() -> None:
    raise ImportError(
        "SigmaGateCallback requires langchain_core.callbacks.BaseCallbackHandler "
        "or langchain.callbacks.base.BaseCallbackHandler"
    )


if BaseCallbackHandler is None:

    class SigmaGateCallback:  # type: ignore[no-redef]
        def __init__(self, *args: Any, **kwargs: Any) -> None:
            _missing_langchain()

else:

    class SigmaGateCallback(BaseCallbackHandler):  # type: ignore[no-redef]
        """After each LLM end, POST text to /v1/verify; raise if action is ABSTAIN."""

        def __init__(
            self,
            cos_host: str = "localhost",
            cos_port: Optional[int] = None,
            model: str = "gemma3:4b",
        ) -> None:
            super().__init__()
            port = cos_port if cos_port is not None else int(os.environ.get("COS_SERVE_PORT", "3001"))
            self.cos = CreationOS(host=cos_host, port=port)
            self.model = model

        def on_llm_end(self, response: Any, **kwargs: Any) -> None:
            text = _llm_output_text(response)
            if not text.strip():
                return
            result = self.cos.verify(text, model=self.model)
            if result.get("action") == "ABSTAIN":
                sig = result.get("sigma", 0.0)
                raise ValueError(
                    f"sigma-gate ABSTAIN: sigma={float(sig):.3f} audit_id={result.get('audit_id')}"
                )
