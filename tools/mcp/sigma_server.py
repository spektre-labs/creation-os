#!/usr/bin/env python3
"""
Creation OS σ tools over MCP (stdio, JSON-RPC 2.0 line protocol).

Exposes:
  sigma_gate   — POST /v1/verify on `cos serve` (text only).
  sigma_verify — one `cos chat --once --json` turn (prompt → response + σ).

Claude Desktop / Cursor example:

  "mcpServers": {
    "creation-os": {
      "command": "python3",
      "args": ["/abs/path/to/creation-os/tools/mcp/sigma_server.py"],
      "env": {"COS_MCP_VERIFY_PORT": "3001"}
    }
  }

Requires `cos serve` on the verify port for sigma_gate; sigma_verify uses the
local `cos` binary in CREATION_OS_ROOT (or parent of this file).
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Dict, Optional

_DEFAULT_SERVER_VERSION = "3.3.0"


def _repo_root() -> Path:
    return Path(__file__).resolve().parent.parent.parent


def _cos_bin() -> str:
    return os.environ.get("COS_MCP_COS_BIN") or str(_repo_root() / "cos")


def _verify_url() -> str:
    host = os.environ.get("COS_MCP_VERIFY_HOST", "127.0.0.1")
    port = os.environ.get("COS_MCP_VERIFY_PORT", "3001")
    return f"http://{host}:{port}/v1/verify"


def _http_verify(text: str, model: Optional[str]) -> Dict[str, Any]:
    body: Dict[str, Any] = {"text": text}
    if model:
        body["model"] = model
    data = json.dumps(body).encode("utf-8")
    req = urllib.request.Request(
        _verify_url(),
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    timeout = float(os.environ.get("COS_MCP_HTTP_TIMEOUT", "60"))
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8", errors="replace"))


def _aggregate_verify(payload: Dict[str, Any]) -> Dict[str, Any]:
    sents = payload.get("sentences") or []
    if not sents:
        return {"sigma": 1.0, "action": "UNKNOWN", "raw": payload}
    max_sig = 0.0
    worst = "ACCEPT"
    rank = {"UNKNOWN": 0, "ACCEPT": 1, "RETHINK": 2, "ABSTAIN": 3}
    for it in sents:
        if not isinstance(it, dict):
            continue
        sig = float(it.get("sigma", 0.0))
        act = str(it.get("action", "UNKNOWN"))
        max_sig = max(max_sig, sig)
        if rank.get(act, 0) >= rank.get(worst, 0):
            worst = act
    return {"sigma": max_sig, "action": worst, "sentences": sents}


def _run_cos_chat_json(prompt: str, model: Optional[str]) -> Dict[str, Any]:
    env = dict(os.environ)
    if model:
        env["COS_BITNET_CHAT_MODEL"] = model
        env["COS_OLLAMA_MODEL"] = model
    cmd = [
        _cos_bin(),
        "chat",
        "--once",
        "--json",
        "--prompt",
        prompt,
    ]
    p = subprocess.run(
        cmd,
        cwd=str(_repo_root()),
        capture_output=True,
        text=True,
        timeout=int(os.environ.get("COS_MCP_CHAT_TIMEOUT", "300")),
        env=env,
    )
    out = (p.stdout or "").strip().splitlines()
    tail = (p.stderr or "")[-4000:]
    if not out:
        return {
            "error": "empty_stdout",
            "returncode": p.returncode,
            "stderr_tail": tail,
        }
    line = out[-1]
    try:
        j = json.loads(line)
    except json.JSONDecodeError:
        return {
            "error": "stdout_not_json",
            "returncode": p.returncode,
            "line": line[:2000],
            "stderr_tail": tail,
        }
    j["returncode"] = p.returncode
    return j


def _tools_list() -> list:
    return [
        {
            "name": "sigma_gate",
            "description": "Verify text via Creation OS HTTP /v1/verify (semantic σ per sentence).",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "text": {"type": "string"},
                    "model": {"type": "string", "description": "Optional chat model id"},
                },
                "required": ["text"],
            },
        },
        {
            "name": "sigma_verify",
            "description": "Full chat turn with σ + action (`cos chat --once --json`).",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "prompt": {"type": "string"},
                    "model": {"type": "string", "description": "Optional model id"},
                },
                "required": ["prompt"],
            },
        },
    ]


def _mcp_tool_result(body: Dict[str, Any]) -> Dict[str, Any]:
    """MCP tools/call result shape (content + isError) for Claude Desktop / Cursor."""
    is_error = isinstance(body, dict) and "error" in body
    text = json.dumps(body, ensure_ascii=False)
    out: Dict[str, Any] = {
        "content": [{"type": "text", "text": text}],
        "isError": bool(is_error),
    }
    if isinstance(body, dict) and not is_error:
        out["structuredContent"] = body
    return out


def _handle_initialize(msg_id: Any, params: Dict[str, Any]) -> Dict[str, Any]:
    ver = os.environ.get("COS_MCP_SERVER_VERSION", _DEFAULT_SERVER_VERSION)
    protocol_version = "2024-11-05"
    if isinstance(params, dict):
        cv = params.get("protocolVersion")
        if isinstance(cv, str) and cv.strip():
            protocol_version = cv.strip()
    return {
        "jsonrpc": "2.0",
        "id": msg_id,
        "result": {
            "protocolVersion": protocol_version,
            "serverInfo": {"name": "creation-os-sigma-mcp", "version": ver},
            "capabilities": {"tools": {"listChanged": False}},
        },
    }


def _handle_tools_list(msg_id: Any) -> Dict[str, Any]:
    return {"jsonrpc": "2.0", "id": msg_id, "result": {"tools": _tools_list()}}


def _handle_tools_call(msg_id: Any, params: Dict[str, Any]) -> Dict[str, Any]:
    name = params.get("name")
    if not name or not isinstance(name, str):
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "error": {"code": -32602, "message": "missing or invalid params.name"},
        }
    args = params.get("arguments")
    if not isinstance(args, dict):
        args = {}

    body: Dict[str, Any]
    if name == "sigma_gate":
        text = str(args.get("text", "") or "").strip()
        if not text:
            body = {"error": "sigma_gate: empty text"}
        else:
            model = args.get("model")
            if model is not None:
                model = str(model)
            try:
                raw = _http_verify(text, model)
                body = _aggregate_verify(raw)
                body["raw"] = raw
            except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError, OSError) as e:
                body = {"error": f"http_failed:{e!s}"}
            except json.JSONDecodeError:
                body = {"error": "bad_json"}
    elif name == "sigma_verify":
        prompt = str(args.get("prompt", "") or "").strip()
        if not prompt:
            body = {"error": "sigma_verify: empty prompt"}
        else:
            model = args.get("model")
            if model is not None:
                model = str(model)
            body = _run_cos_chat_json(prompt, model)
    else:
        body = {"error": "unknown tool", "name": name}

    return {
        "jsonrpc": "2.0",
        "id": msg_id,
        "result": _mcp_tool_result(body),
    }


def _dispatch_line(msg: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    if msg.get("jsonrpc") != "2.0":
        return None
    has_id = "id" in msg
    msg_id = msg.get("id")
    method = msg.get("method")
    if not has_id:
        # JSON-RPC notifications (no response).
        if isinstance(method, str) and method.startswith("notifications/"):
            return None
        return None
    if not method:
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "error": {"code": -32600, "message": "missing method"},
        }
    if method == "initialize":
        p = msg.get("params")
        if not isinstance(p, dict):
            p = {}
        return _handle_initialize(msg_id, p)
    if method == "tools/list":
        return _handle_tools_list(msg_id)
    if method == "tools/call":
        p = msg.get("params")
        if not isinstance(p, dict):
            p = {}
        return _handle_tools_call(msg_id, p)
    if method == "ping":
        return {"jsonrpc": "2.0", "id": msg_id, "result": {"pong": True}}
    return {
        "jsonrpc": "2.0",
        "id": msg_id,
        "error": {"code": -32601, "message": "method_not_found"},
    }


def main() -> None:
    while True:
        line = sys.stdin.readline()
        if line == "":
            break
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            sys.stdout.write(
                json.dumps(
                    {
                        "jsonrpc": "2.0",
                        "id": None,
                        "error": {"code": -32700, "message": "parse error"},
                    }
                )
                + "\n"
            )
            sys.stdout.flush()
            continue
        if not isinstance(msg, dict):
            continue
        reply = _dispatch_line(msg)
        if reply is not None:
            sys.stdout.write(json.dumps(reply, ensure_ascii=False) + "\n")
            sys.stdout.flush()


if __name__ == "__main__":
    main()
