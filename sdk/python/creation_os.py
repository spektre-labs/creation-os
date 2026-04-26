"""Thin HTTP client for cos-serve (σ-gate API).

Requires: pip install requests
Server: build with `make cos-serve`, run `./cos-serve --port 3001` or `cos serve`.
"""

from __future__ import annotations

import os
from typing import Any, Dict, Optional

try:
    import requests
except ImportError as e:  # pragma: no cover
    raise ImportError(
        "creation_os requires the 'requests' package: pip install requests"
    ) from e


class CreationOS:
    """JSON client for POST /v1/gate, POST /v1/verify, GET /v1/health."""

    def __init__(
        self,
        host: str = "localhost",
        port: Optional[int] = None,
        *,
        base_url: Optional[str] = None,
        timeout_s: float = 600.0,
    ) -> None:
        if base_url is not None:
            self.base = base_url.rstrip("/")
        else:
            if port is None:
                port = int(os.environ.get("COS_SERVE_PORT", "3001"))
            self.base = f"http://{host}:{port}"
        self.timeout_s = timeout_s

    def gate(self, prompt: str, model: str = "gemma3:4b") -> Dict[str, Any]:
        r = requests.post(
            f"{self.base}/v1/gate",
            json={"prompt": prompt, "model": model},
            timeout=self.timeout_s,
        )
        r.raise_for_status()
        return r.json()

    def verify(self, text: str, model: str = "gemma3:4b") -> Dict[str, Any]:
        r = requests.post(
            f"{self.base}/v1/verify",
            json={"text": text, "model": model},
            timeout=self.timeout_s,
        )
        r.raise_for_status()
        return r.json()

    def health(self) -> Dict[str, Any]:
        r = requests.get(f"{self.base}/v1/health", timeout=30.0)
        r.raise_for_status()
        return r.json()

    def audit(self, audit_id: str) -> Dict[str, Any]:
        r = requests.get(f"{self.base}/v1/audit/{audit_id}", timeout=30.0)
        r.raise_for_status()
        return r.json()
