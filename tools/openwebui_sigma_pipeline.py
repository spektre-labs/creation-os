# -*- coding: utf-8 -*-
"""
Open WebUI **Filter** — append a Creation OS σ footer using `cos_serve`.

Install (typical Open WebUI ≥0.3):
  Admin → Functions → **Filters** → **+** → paste this file → enable on a model.

Requires `python3 scripts/cos_serve.py` (or a systemd unit) reachable from the
Open WebUI container (adjust **Valves.cos_serve_url**).

**Semantics:** the footer σ comes from `cos chat --once` on the **last user**
message only. It is **not** guaranteed to match the σ of the upstream model’s
reply unless that stack matches Creation OS.
"""
from __future__ import annotations

import json
import urllib.error
import urllib.request
from typing import Any, Dict, Optional

from pydantic import BaseModel, Field


class Filter:
    class Valves(BaseModel):
        cos_serve_url: str = Field(
            default="http://127.0.0.1:3001",
            description="Base URL of scripts/cos_serve.py",
        )
        timeout_s: int = Field(default=120, ge=5, le=900)

    def __init__(self) -> None:
        self.valves = self.Valves()

    async def inlet(self, body: Dict[str, Any], __user__: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        return body

    async def outlet(self, body: Dict[str, Any], __user__: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        url = self.valves.cos_serve_url.rstrip("/")
        messages = list(body.get("messages") or [])
        prompt = ""
        for m in reversed(messages):
            if isinstance(m, dict) and m.get("role") == "user":
                prompt = str(m.get("content") or "")
                break
        if not prompt.strip():
            body["messages"] = messages
            return body
        payload = json.dumps({"prompt": prompt, "multi_sigma": True, "verbose": True}).encode("utf-8")
        req = urllib.request.Request(
            url + "/v1/chat",
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=self.valves.timeout_s) as resp:
                data = json.loads(resp.read().decode("utf-8"))
        except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError, json.JSONDecodeError, OSError) as e:
            tail = "\n\n---\n_σ = N/A | Creation OS (cos_serve error: %s)_" % (e,)
        else:
            s = data.get("sigma")
            act = data.get("action", "N/A")
            tail = "\n\n---\n_σ = %s | %s | Creation OS_" % (s if s is not None else "N/A", act)
        for m in reversed(messages):
            if isinstance(m, dict) and m.get("role") == "assistant":
                m["content"] = str(m.get("content") or "") + tail
                break
        body["messages"] = messages
        return body
