#!/usr/bin/env python3
"""
Creation OS MCP Server (Python, stdio JSON-RPC 2.0).

Exposes σ-measured tools that delegate to the `cos` C binary (`cos chat`,
`cos introspect`). Wire shape matches the in-repo C `cos-mcp` server
(flat `result` objects, protocolVersion 2026-03-26), with Python-specific
tool names: sigma_chat, sigma_measure, sigma_health.

For the native in-process pipeline (cos.chat, cos.sigma, …), use ./cos-mcp.
"""
from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

COS_MCP2_PROTOCOL_VERSION = "2026-03-26"
_DEFAULT_SERVER_VERSION = "3.3.0"


def _repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def _cos_bin() -> str:
    return os.environ.get("COS_SERVE_COS_BIN") or str(_repo_root() / "cos")


def _load_cos_serve() -> Any:
    path = _repo_root() / "scripts" / "cos_serve.py"
    spec = importlib.util.spec_from_file_location("cos_serve", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load scripts/cos_serve.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


_cs: Optional[Any] = None


def _cos_serve_mod() -> Any:
    global _cs
    if _cs is None:
        _cs = _load_cos_serve()
    return _cs


def _chat_env_extra() -> Optional[Dict[str, str]]:
    if "COS_ENGRAM_DISABLE" not in os.environ:
        return None
    return {"COS_ENGRAM_DISABLE": os.environ["COS_ENGRAM_DISABLE"]}


def _run_chat_parsed(prompt: str, timeout: int = 300) -> Tuple[int, Dict[str, Any]]:
    cs = _cos_serve_mod()
    rc, so, se = cs._run_cos_chat(  # noqa: SLF001
        prompt,
        multi_sigma=True,
        verbose=True,
        cwd=_repo_root(),
        timeout=timeout,
        env_extra=_chat_env_extra(),
    )
    merged = so + ("\n" + se if se else "")
    parsed = cs.parse_cos_chat_output(merged)
    parsed["returncode"] = rc
    return rc, parsed


def _sigma_health() -> Dict[str, Any]:
    p = subprocess.run(
        [_cos_bin(), "introspect"],
        cwd=str(_repo_root()),
        capture_output=True,
        text=True,
        timeout=60,
        env=dict(os.environ),
    )
    text = (p.stdout or "") + ("\n" + p.stderr if p.stderr else "")
    text = text.strip()
    if not text:
        text = "OK" if p.returncode == 0 else "(empty introspect output)"
    return {"report": text, "returncode": p.returncode}


def _tools_list() -> list:
    return [
        {
            "name": "sigma_chat",
            "description": (
                "Ask a question with σ uncertainty measurement; returns answer, σ, action."
            ),
            "inputSchema": {
                "type": "object",
                "properties": {
                    "prompt": {
                        "type": "string",
                        "description": "The question or prompt",
                    },
                },
                "required": ["prompt"],
            },
        },
        {
            "name": "sigma_measure",
            "description": (
                "Run a σ-gated chat turn on the given text to surface σ and action "
                "(no separate measure-only path in the C CLI)."
            ),
            "inputSchema": {
                "type": "object",
                "properties": {
                    "text": {
                        "type": "string",
                        "description": "Text to score through the pipeline",
                    },
                },
                "required": ["text"],
            },
        },
        {
            "name": "sigma_health",
            "description": "Creation OS introspection snapshot (`cos introspect`).",
            "inputSchema": {"type": "object", "properties": {}},
        },
    ]


def _handle_initialize(msg_id: Any) -> Dict[str, Any]:
    ver = os.environ.get("COS_MCP_SERVER_VERSION", _DEFAULT_SERVER_VERSION)
    return {
        "jsonrpc": "2.0",
        "id": msg_id,
        "result": {
            "protocolVersion": COS_MCP2_PROTOCOL_VERSION,
            "serverInfo": {
                "name": "creation-os-sigma",
                "version": ver,
            },
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

    if name == "sigma_chat":
        prompt = str(args.get("prompt", "") or "").strip()
        if not prompt:
            body: Dict[str, Any] = {"error": "sigma_chat: empty prompt"}
        else:
            rc, p = _run_chat_parsed(prompt)
            sig = p.get("sigma")
            body = {
                "response": p.get("response") or "",
                "sigma": float(sig) if sig is not None else -1.0,
                "action": p.get("action") or "UNKNOWN",
                "returncode": rc,
            }
            if p.get("route") is not None:
                body["route"] = p["route"]
            for k in (
                "sigma_semantic",
                "sigma_logprob",
                "sigma_entropy",
                "sigma_perplexity",
                "sigma_consistency",
                "multi_k",
            ):
                if p.get(k) is not None:
                    body[k] = p[k]
            if rc != 0:
                body["stderr_hint"] = (p.get("raw_tail") or "")[:2000]
    elif name == "sigma_measure":
        text = str(args.get("text", "") or "").strip()
        if not text:
            body = {"error": "sigma_measure: empty text"}
        else:
            rc, p = _run_chat_parsed(
                "Evaluate this claim (brief answer); uncertainty is logged in σ.\n\n"
                + text
            )
            sig = p.get("sigma")
            body = {
                "sigma": float(sig) if sig is not None else -1.0,
                "action": p.get("action") or "UNKNOWN",
                "assistant_preview": (p.get("response") or "")[:800],
                "returncode": rc,
            }
    elif name == "sigma_health":
        body = _sigma_health()
    else:
        body = {"error": "unknown tool", "name": name}

    return {"jsonrpc": "2.0", "id": msg_id, "result": body}


def _handle_ping(msg_id: Any) -> Dict[str, Any]:
    return {"jsonrpc": "2.0", "id": msg_id, "result": {"pong": True}}


def _dispatch_line(msg: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    if msg.get("jsonrpc") != "2.0":
        return None
    if "id" not in msg:
        return None
    msg_id = msg["id"]
    method = msg.get("method")
    if not method:
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "error": {"code": -32600, "message": "missing method"},
        }
    if method == "initialize":
        return _handle_initialize(msg_id)
    if method == "tools/list":
        return _handle_tools_list(msg_id)
    if method == "tools/call":
        params = msg.get("params")
        if not isinstance(params, dict):
            params = {}
        return _handle_tools_call(msg_id, params)
    if method == "ping":
        return _handle_ping(msg_id)
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
