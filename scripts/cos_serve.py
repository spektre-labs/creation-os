#!/usr/bin/env python3
"""
Thin HTTP wrapper around `cos chat --once` for local browser / Open WebUI.

  python3 scripts/cos_serve.py --port 3001

Endpoints:
  GET  /health          JSON {status, model, ...}
  GET  /stats           JSON from `cos cost --json` when available
  POST /v1/chat         JSON body: prompt, multi_sigma (bool), verbose (bool)
  OPTIONS /*            CORS preflight

Environment (optional, forwarded to cos chat / BitNet HTTP):
  COS_BITNET_SERVER_EXTERNAL, COS_BITNET_SERVER_PORT, COS_BITNET_CHAT_MODEL, ...
  COS_SERVE_COS_BIN       absolute path to `cos` binary
  COS_SERVE_REPO_ROOT     cwd for subprocess (default: parent of scripts/)
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from typing import Any, Dict, Optional, Tuple


def _repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def _cos_bin() -> str:
    return os.environ.get("COS_SERVE_COS_BIN") or str(_repo_root() / "cos")


def _subprocess_env(extra: Optional[Dict[str, str]] = None) -> Dict[str, str]:
    env = dict(os.environ)
    env.setdefault("COS_BITNET_SERVER_EXTERNAL", "1")
    env.setdefault("COS_BITNET_SERVER_PORT", "11434")
    env.setdefault("COS_BITNET_CHAT_MODEL", "gemma3:4b")
    # Deterministic parse path: full body on one line, not stream interleaving.
    env["COS_BITNET_STREAM"] = "0"
    if extra:
        env.update(extra)
    return env


def _run_cos_chat(
    prompt: str,
    *,
    multi_sigma: bool,
    verbose: bool,
    cwd: Path,
    timeout: int,
) -> Tuple[int, str, str]:
    cmd = [
        _cos_bin(),
        "chat",
        "--once",
        "--prompt",
        prompt,
        "--no-stream",
        "--no-transcript",
        "--no-coherence",
    ]
    if multi_sigma:
        cmd.append("--multi-sigma")
    if verbose:
        cmd.append("--verbose")

    p = subprocess.run(
        cmd,
        cwd=str(cwd),
        capture_output=True,
        text=True,
        timeout=timeout,
        env=_subprocess_env(),
    )
    return p.returncode, p.stdout, p.stderr


_re_round_tail = re.compile(
    r"\[σ_peak=([0-9.]+)\s+action=(\S+)\s+route=(\S+)\]",
)
_re_receipt = re.compile(
    r"\[σ=([0-9.]+)\s*\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|\s*([^|\]]+)",
)
_re_speed = re.compile(
    r"\[speed:\s*(\d+)\s*tok\s*\|\s*([0-9.]+)ms",
)
_re_multi = re.compile(
    r"\[σ_combined=([0-9.]+)\s*\|\s*σ_semantic=([0-9.]+)\s*\|\s*σ_logprob=([0-9.]+)"
    r"\s*σ_entropy=([0-9.]+)\s*σ_perplexity=([0-9.]+)\s*σ_consistency=([0-9.]+)\s*\|\s*k=(\d+)\]",
)
_re_coherence = re.compile(r"K_eff=([0-9.]+)")
_re_meta = re.compile(
    r"\[meta:\s*perception=([0-9.]+)\s*self=([0-9.]+)\s*social=([0-9.]+)\s*situational=([0-9.]+)\]",
)


def parse_cos_chat_output(
    text: str,
) -> Dict[str, Any]:
    """Best-effort parse of cos-chat stdout+stderr."""
    blob = text
    out: Dict[str, Any] = {
        "response": "",
        "sigma": None,
        "sigma_semantic": None,
        "sigma_logprob": None,
        "sigma_entropy": None,
        "sigma_perplexity": None,
        "sigma_consistency": None,
        "action": None,
        "attribution": "NONE",
        "k_eff": None,
        "route": None,
        "model": os.environ.get("COS_BITNET_CHAT_MODEL")
        or os.environ.get("COS_OLLAMA_MODEL")
        or "gemma3:4b",
        "latency_ms": None,
        "tokens": None,
        "multi_k": None,
        "raw_tail": blob[-4000:] if len(blob) > 4000 else blob,
    }

    peak = _re_round_tail.search(blob)
    if peak:
        out["round0_sigma_peak"] = float(peak.group(1))
        out["round0_route"] = peak.group(3).strip()
        idx = blob.find("[σ_peak=")
        head = blob[:idx] if idx >= 0 else blob
        if "round 0" in head:
            tail = head.split("round 0", 1)[-1].strip()
            if tail.startswith("(streamed above)"):
                out["response"] = ""
                out["parse_note"] = "stream_placeholder_receipt_only"
            else:
                out["response"] = tail

    ms = _re_speed.search(blob)
    if ms:
        try:
            out["tokens"] = int(ms.group(1))
            out["latency_ms"] = float(ms.group(2))
        except ValueError:
            pass

    mr = _re_receipt.search(blob)
    if mr:
        try:
            out["sigma"] = float(mr.group(1))
        except ValueError:
            pass
        out["action"] = mr.group(2).strip()
        out["route"] = mr.group(4).strip()
    else:
        for line in blob.splitlines():
            s = line.strip()
            if not s.startswith("[σ=") or "|" not in s:
                continue
            inner = s.strip("[]")
            parts = [p.strip() for p in inner.split("|")]
            if len(parts) >= 4 and parts[0].startswith("σ="):
                try:
                    out["sigma"] = float(parts[0].split("=", 1)[1])
                except ValueError:
                    pass
                out["action"] = parts[1]
                out["route"] = parts[3]
                break

    mm = _re_multi.search(blob)
    if mm:
        try:
            out["sigma_combined"] = float(mm.group(1))
            out["sigma_semantic"] = float(mm.group(2))
            out["sigma_logprob"] = float(mm.group(3))
            out["sigma_entropy"] = float(mm.group(4))
            out["sigma_perplexity"] = float(mm.group(5))
            out["sigma_consistency"] = float(mm.group(6))
            out["multi_k"] = int(mm.group(7))
        except ValueError:
            pass

    mc = _re_coherence.search(blob)
    if mc:
        try:
            out["k_eff"] = float(mc.group(1))
        except ValueError:
            pass

    mmeta = _re_meta.search(blob)
    if mmeta:
        out["meta"] = {
            "perception": float(mmeta.group(1)),
            "self": float(mmeta.group(2)),
            "social": float(mmeta.group(3)),
            "situational": float(mmeta.group(4)),
        }

    if out["sigma"] is None and out.get("sigma_combined") is not None:
        out["sigma"] = out["sigma_combined"]

    return out


def cos_cost_json(cwd: Path, timeout: int = 30) -> Any:
    try:
        p = subprocess.run(
            [_cos_bin(), "cost", "--json"],
            cwd=str(cwd),
            capture_output=True,
            text=True,
            timeout=timeout,
            env=dict(os.environ),
        )
        if p.returncode != 0 or not p.stdout.strip():
            return {"error": "cos cost --json failed", "stderr": p.stderr[:2000]}
        return json.loads(p.stdout)
    except (subprocess.TimeoutExpired, json.JSONDecodeError, OSError) as e:
        return {"error": str(e)}


class CosHTTPHandler(BaseHTTPRequestHandler):
    server_version = "CreationOsCosServe/0.1"

    def _cors(self) -> None:
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def do_OPTIONS(self) -> None:  # noqa: N802
        self.send_response(204)
        self._cors()
        self.end_headers()

    def log_message(self, fmt: str, *args: Any) -> None:
        sys.stderr.write("%s - - [%s] %s\n" % (self.address_string(), self.log_date_time_string(), fmt % args))

    def do_GET(self) -> None:  # noqa: N802
        root = _repo_root()
        if self.path.startswith("/health"):
            model = os.environ.get("COS_BITNET_CHAT_MODEL") or os.environ.get(
                "COS_OLLAMA_MODEL", "gemma3:4b"
            )
            body = {
                "status": "ok",
                "model": model,
                "sigma_mean": None,
                "cos_bin": _cos_bin(),
            }
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self._cors()
            self.end_headers()
            self.wfile.write(json.dumps(body).encode("utf-8"))
            return
        if self.path.startswith("/stats"):
            data = cos_cost_json(root)
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self._cors()
            self.end_headers()
            self.wfile.write(json.dumps(data).encode("utf-8"))
            return
        self.send_error(404, "Not Found")

    def do_POST(self) -> None:  # noqa: N802
        if not self.path.startswith("/v1/chat"):
            self.send_error(404, "Not Found")
            return
        try:
            n = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            n = 0
        raw = self.rfile.read(n) if n > 0 else b"{}"
        try:
            req = json.loads(raw.decode("utf-8") or "{}")
        except json.JSONDecodeError:
            self.send_response(400)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self._cors()
            self.end_headers()
            self.wfile.write(b'{"error":"invalid JSON"}')
            return

        prompt = str(req.get("prompt", "") or "")
        multi = bool(req.get("multi_sigma", True))
        verbose = bool(req.get("verbose", True))
        timeout = int(req.get("timeout_s", 300))

        root = Path(os.environ.get("COS_SERVE_REPO_ROOT") or _repo_root())
        rc, so, se = _run_cos_chat(prompt, multi_sigma=multi, verbose=verbose, cwd=root, timeout=timeout)
        merged = so + ("\n" + se if se else "")
        parsed = parse_cos_chat_output(merged)
        parsed["returncode"] = rc
        if rc != 0:
            parsed.setdefault("error", "cos chat non-zero exit")
        self.send_response(200 if rc == 0 else 502)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self._cors()
        self.end_headers()
        self.wfile.write(json.dumps(parsed, indent=2).encode("utf-8"))


def main() -> None:
    ap = argparse.ArgumentParser(description="HTTP shim for cos chat --once")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=3001)
    ns = ap.parse_args()
    httpd = HTTPServer((ns.host, ns.port), CosHTTPHandler)
    print(
        f"cos_serve listening on http://{ns.host}:{ns.port} "
        f"(cwd chats use {_repo_root()})",
        flush=True,
    )
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nshutdown", flush=True)


if __name__ == "__main__":
    main()
