# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-dashboard — JSON HTTP API for σ-observe (local-first).

Frontend is out of tree; this module serves JSON only plus a legacy HTML helper for ``cos monitor``.
"""
from __future__ import annotations

import json
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Any, Dict, List, Type

from .sigma_observe import SigmaObserve


def _json_bytes(data: Any, status: int = 200) -> bytes:
    payload = json.dumps(data, ensure_ascii=False, default=str) + "\n"
    return payload.encode("utf-8")


class SigmaDashboard:
    """Legacy static HTML generator for ``cos monitor --html``."""

    def generate_html(self, traced: List[Dict[str, Any]]) -> str:
        rows = []
        for t in traced:
            if not isinstance(t, dict):
                continue
            sig = t.get("normalized_sigma", t.get("sigma", ""))
            vd = t.get("verdict", "")
            rows.append(f"<tr><td>{t.get('step', '')}</td><td>{sig}</td><td>{vd}</td></tr>")
        body = "\n".join(rows) if rows else "<tr><td colspan='3'>empty</td></tr>"
        return (
            "<!doctype html><html><head><meta charset='utf-8'><title>σ monitor</title>"
            "<style>table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:4px}</style>"
            f"</head><body><table><thead><tr><th>step</th><th>σ</th><th>verdict</th></thead><tbody>{body}</tbody></table></body></html>"
        )


def _make_handler_class(observer: SigmaObserve) -> Type[BaseHTTPRequestHandler]:
    obs = observer

    class SigmaDashboardHandler(BaseHTTPRequestHandler):
        def log_message(self, fmt: str, *args: Any) -> None:
            return

        def _send(self, code: int, body: bytes) -> None:
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self) -> None:
            path = self.path.split("?", 1)[0]
            if path == "/v1/health":
                self._send(200, _json_bytes({"status": "ok"}))
            elif path == "/v1/dashboard":
                self._send(200, _json_bytes(obs.dashboard()))
            elif path == "/v1/traces":
                traces = list(obs.traces)[-50:]
                self._send(200, _json_bytes({"traces": traces}))
            elif path == "/v1/alerts":
                alerts = obs.alerts.active_alerts[-20:]
                self._send(200, _json_bytes({"alerts": alerts}))
            else:
                self._send(404, _json_bytes({"error": "not found"}, status=404))

        def do_POST(self) -> None:
            path = self.path.split("?", 1)[0]
            if path != "/v1/record":
                self._send(404, _json_bytes({"error": "not found"}, status=404))
                return
            try:
                length = int(self.headers.get("Content-Length", "0") or "0")
            except ValueError:
                length = 0
            raw = self.rfile.read(length) if length > 0 else b"{}"
            try:
                data = json.loads(raw.decode("utf-8") or "{}")
            except json.JSONDecodeError:
                self._send(400, _json_bytes({"ok": False, "error": "invalid json"}))
                return
            if not isinstance(data, dict):
                self._send(400, _json_bytes({"ok": False, "error": "body must be object"}))
                return
            prompt = str(data.get("prompt", ""))
            response = str(data.get("response", ""))
            model = str(data.get("model", "http"))
            meta = data.get("metadata")
            md = meta if isinstance(meta, dict) else None
            if obs.gate is None:
                self._send(503, _json_bytes({"ok": False, "error": "observer has no gate"}))
                return
            rec = obs.trace_llm(prompt, response, model, metadata=md)
            self._send(200, _json_bytes({"ok": True, "record": rec}))

    return SigmaDashboardHandler


def serve_dashboard(
    observer: SigmaObserve,
    *,
    host: str = "0.0.0.0",
    port: int = 3003,
) -> None:
    """Blocking HTTP server (stdlib only)."""
    handler = _make_handler_class(observer)
    server = HTTPServer((host, int(port)), handler)
    print(f"σ-dashboard: http://127.0.0.1:{port}/v1/dashboard", flush=True)
    server.serve_forever()


__all__ = ["SigmaDashboard", "serve_dashboard"]
