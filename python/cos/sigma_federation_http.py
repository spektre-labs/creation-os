# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Minimal stdlib HTTP surface for ``cos federation --server`` (lab only)."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import json
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from typing import Any, Dict


def serve_federation_http(*, host: str, port: int, workspace: str) -> HTTPServer:
    ws = Path(workspace).expanduser()
    ws.mkdir(parents=True, exist_ok=True)
    state_path = ws / "fed_lab_state.json"

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt: str, *args: Any) -> None:
            return

        def _send_json(self, code: int, body: Dict[str, Any]) -> None:
            raw = json.dumps(body, ensure_ascii=False).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)

        def do_GET(self) -> None:  # noqa: N802
            if self.path.startswith("/status"):
                if state_path.is_file():
                    try:
                        self._send_json(200, json.loads(state_path.read_text(encoding="utf-8")))
                    except json.JSONDecodeError:
                        self._send_json(200, {"error": "invalid fed_lab_state.json"})
                else:
                    self._send_json(200, {})
                return
            self._send_json(404, {"error": "not found"})

        def do_POST(self) -> None:  # noqa: N802
            if self.path == "/join":
                length = int(self.headers.get("Content-Length", "0") or 0)
                _ = self.rfile.read(length) if length > 0 else b""
                self._send_json(200, {"ok": True, "note": "join acknowledged (lab stub)"})
                return
            if self.path == "/train":
                length = int(self.headers.get("Content-Length", "0") or 0)
                raw = self.rfile.read(length) if length > 0 else b"{}"
                try:
                    obj = json.loads(raw.decode("utf-8"))
                except json.JSONDecodeError:
                    obj = {}
                rounds = int(obj.get("rounds", 1) or 1)
                from cos.sigma_federated import run_mock_federation_lab

                out = run_mock_federation_lab(rounds=rounds, workspace=str(ws))
                self._send_json(200, out)
                return
            self._send_json(404, {"error": "not found"})

    return HTTPServer((host, int(port)), Handler)


__all__ = ["serve_federation_http"]
