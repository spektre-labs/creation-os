#!/usr/bin/env python3
"""
Serve Creation OS Mirror + /api/hw_proof (cntpct AMX pulse + bit-exact 1.0 from dylib).
Run: CREATION_OS_RT_CONSTRAINT=1 python3 mirror_hw_sync.py
Then open http://127.0.0.1:8765/
"""
from __future__ import annotations

import json
import os
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

_ROOT = Path(__file__).resolve().parent


def _hw_proof() -> dict:
    try:
        import bare_metal_infer as b
    except Exception as e:
        return {"ok": False, "error": f"import:{type(e).__name__}"}
    d = b.bare_hw_proof_amx_pulse()
    if d is None:
        return {"ok": False, "error": "no_dylib_or_symbol"}
    return {"ok": True, **d}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args) -> None:
        return

    def do_GET(self) -> None:
        if self.path == "/" or self.path == "/creation_os_mirror.html":
            p = _ROOT / "creation_os_mirror.html"
            body = p.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if self.path == "/api/hw_proof":
            payload = _hw_proof()
            body = json.dumps(payload).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        self.send_error(404)


def main() -> None:
    port = int(os.environ.get("CREATION_OS_MIRROR_PORT", "8765"))
    host = os.environ.get("CREATION_OS_MIRROR_HOST", "127.0.0.1")
    httpd = HTTPServer((host, port), Handler)
    print(f"Mirror + hw_proof: http://{host}:{port}/")
    httpd.serve_forever()


if __name__ == "__main__":
    main()
