#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""HTTP proxy: OpenAI /v1/completions -> upstream, normalize logprobs for lm-eval.

Eleuther ``local-completions`` expects legacy shapes::

  choices[0].logprobs.token_logprobs: list[float]
  choices[0].logprobs.top_logprobs: list[dict[str, float]]  # token -> logprob

Some llama.cpp OpenAI servers return only the chat-style layout::

  choices[0].logprobs.content: [{token, logprob, top_logprobs: [...]}, ...]

This proxy forwards JSON to UPSTREAM (e.g. llama-server) and rewrites responses
when needed. If the request used echo=true, it also prefixes the request prompt
onto choices[].text when the upstream omitted the prefix (lm-eval aligns
ctxlens with the full echoed string).
"""
from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Any, Dict, List, Tuple


def _normalize_choice_logprobs(choice: Dict[str, Any]) -> None:
    lp = choice.get("logprobs")
    if lp is None:
        return
    if "token_logprobs" in lp and "top_logprobs" in lp:
        return
    content = lp.get("content")
    if not isinstance(content, list) or not content:
        return
    token_logprobs: List[float] = []
    top_logprobs: List[Dict[str, float]] = []
    for item in content:
        if not isinstance(item, dict):
            continue
        tok = item.get("token", "")
        tlp = float(item.get("logprob", 0.0))
        token_logprobs.append(tlp)
        alt_map: Dict[str, float] = {}
        for alt in item.get("top_logprobs") or []:
            if isinstance(alt, dict) and "token" in alt:
                alt_map[str(alt["token"])] = float(alt.get("logprob", 0.0))
        alt_map[str(tok)] = tlp
        top_logprobs.append(alt_map)
    lp["token_logprobs"] = token_logprobs
    lp["top_logprobs"] = top_logprobs


def _rewrite_response(body: bytes, echo_prompt: str | None) -> bytes:
    try:
        data = json.loads(body.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return body
    choices = data.get("choices")
    if not isinstance(choices, list):
        return body
    for ch in choices:
        if not isinstance(ch, dict):
            continue
        _normalize_choice_logprobs(ch)
        if echo_prompt and isinstance(ch.get("text"), str):
            t = ch["text"]
            if not t.startswith(echo_prompt):
                ch["text"] = echo_prompt + t
    return json.dumps(data, ensure_ascii=False).encode("utf-8")


def _forward(method: str, upstream: str, path: str, headers: List[Tuple[str, str]], body: bytes) -> Tuple[int, bytes, str]:
    url = upstream.rstrip("/") + path
    req = urllib.request.Request(url, data=body if body else None, method=method)
    for k, v in headers:
        lk = k.lower()
        if lk in ("host", "content-length"):
            continue
        req.add_header(k, v)
    try:
        with urllib.request.urlopen(req, timeout=600) as resp:
            ctype = resp.headers.get("Content-Type", "application/json")
            return resp.getcode(), resp.read(), ctype
    except urllib.error.HTTPError as e:
        return e.code, e.read(), e.headers.get("Content-Type", "application/json")


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"
    upstream: str = os.environ.get("LM_EVAL_PROXY_UPSTREAM", "http://127.0.0.1:18089")

    def log_message(self, fmt: str, *args: Any) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def do_GET(self) -> None:  # noqa: N802
        self._proxy("GET", None)

    def do_POST(self) -> None:  # noqa: N802
        n = int(self.headers.get("Content-Length", "0") or 0)
        body = self.rfile.read(n) if n > 0 else b""
        self._proxy("POST", body)

    def _proxy(self, method: str, body: bytes | None) -> None:
        echo_prompt: str | None = None
        if body:
            try:
                jd = json.loads(body.decode("utf-8"))
                if jd.get("echo") and isinstance(jd.get("prompt"), str):
                    echo_prompt = jd["prompt"]
            except (UnicodeDecodeError, json.JSONDecodeError):
                pass
        hdrs = [(k, v) for k, v in self.headers.items()]
        code, out, ctype = _forward(method, self.upstream, self.path, hdrs, body or b"")
        if self.path == "/v1/completions" and code == 200 and echo_prompt:
            out = _rewrite_response(out, echo_prompt)
        self.send_response(code)
        self.send_header("Content-Type", ctype.split(";")[0])
        self.send_header("Content-Length", str(len(out)))
        self.end_headers()
        self.wfile.write(out)


def main() -> int:
    host = os.environ.get("LM_EVAL_PROXY_HOST", "127.0.0.1")
    port = int(os.environ.get("LM_EVAL_PROXY_PORT", "18090"))
    Handler.upstream = os.environ.get("LM_EVAL_PROXY_UPSTREAM", "http://127.0.0.1:18089")
    httpd = HTTPServer((host, port), Handler)
    print("lm_eval_logprobs_proxy listening on http://%s:%s -> %s" % (host, port, Handler.upstream), flush=True)
    httpd.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
