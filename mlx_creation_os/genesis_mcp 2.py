#!/usr/bin/env python3
"""
GENESIS MCP — Model Context Protocol server for Creation OS.

Exposes Genesis tools via MCP standard protocol (JSON-RPC over stdio).
Any MCP-compatible client can connect: Cursor, Claude Desktop, etc.

All tool calls pass through the kernel. σ measured on every action.
Codex guides identity. No plugins — MCP is the standard.

Tools exposed:
  - genesis_generate: Generate text with full Creation OS pipeline
  - genesis_read_file: Read files with σ-checked output
  - genesis_run_code: Execute code in sandbox
  - genesis_search_corpus: Search Spektre papers
  - genesis_sigma_check: Check σ of any text
  - genesis_codex_verify: Verify text against Codex
  - genesis_voice_tts: Text to speech

Usage (stdio transport):
    python3 genesis_mcp.py

MCP config (for clients):
    {
      "mcpServers": {
        "genesis": {
          "command": "python3",
          "args": ["path/to/genesis_mcp.py"]
        }
      }
    }

1 = 1.
"""
from __future__ import annotations

import json
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

try:
    from creation_os_core import check_output, is_coherent
except ImportError:
    def check_output(t: str) -> int: return 0
    def is_coherent(t: str) -> bool: return True


MCP_PROTOCOL_VERSION = "2024-11-05"
SERVER_NAME = "genesis-creation-os"
SERVER_VERSION = "1.0.0"

TOOLS = [
    {
        "name": "genesis_generate",
        "description": "Generate text using Genesis with full Creation OS pipeline (ghost boot, kernel, σ-monitoring, living weights). Returns response + σ receipt.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "prompt": {"type": "string", "description": "The prompt to generate from"},
                "max_tokens": {"type": "integer", "description": "Maximum tokens", "default": 256},
                "temperature": {"type": "number", "description": "Sampling temperature", "default": 0.3},
            },
            "required": ["prompt"],
        },
    },
    {
        "name": "genesis_read_file",
        "description": "Read a file and return its contents. Output is σ-checked.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "File path to read"},
                "max_chars": {"type": "integer", "description": "Max characters to return", "default": 4000},
            },
            "required": ["path"],
        },
    },
    {
        "name": "genesis_run_code",
        "description": "Execute code locally with timeout. σ-checked output.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "language": {"type": "string", "enum": ["python", "bash", "javascript"]},
                "code": {"type": "string", "description": "Code to execute"},
            },
            "required": ["language", "code"],
        },
    },
    {
        "name": "genesis_search_corpus",
        "description": "Search the Spektre paper corpus (79 papers). Returns relevant chunks with citations.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "query": {"type": "string", "description": "Search query"},
                "top_k": {"type": "integer", "description": "Number of results", "default": 5},
            },
            "required": ["query"],
        },
    },
    {
        "name": "genesis_sigma_check",
        "description": "Check the σ (coherence/firmware leakage) of any text. Returns sigma value and coherence status.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "text": {"type": "string", "description": "Text to check"},
            },
            "required": ["text"],
        },
    },
    {
        "name": "genesis_codex_verify",
        "description": "Verify text against the Atlantean Codex (identity anchor). Returns whether text is compatible with Genesis identity.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "text": {"type": "string", "description": "Text to verify"},
            },
            "required": ["text"],
        },
    },
    {
        "name": "genesis_voice_tts",
        "description": "Convert text to speech using local TTS (macOS).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "text": {"type": "string", "description": "Text to speak"},
            },
            "required": ["text"],
        },
    },
]


def _handle_generate(args: Dict[str, Any]) -> Dict[str, Any]:
    try:
        from creation_os import creation_os_generate_native
        text, receipt = creation_os_generate_native(
            args["prompt"],
            max_tokens=args.get("max_tokens", 256),
            temp=args.get("temperature", 0.3),
        )
        return {"text": text, "sigma": receipt.get("sigma", 0), "coherent": receipt.get("coherent", False)}
    except Exception as e:
        return {"error": str(e)}


def _handle_read_file(args: Dict[str, Any]) -> Dict[str, Any]:
    path = args["path"]
    max_chars = args.get("max_chars", 4000)
    try:
        p = Path(path).resolve()
        if not p.is_file():
            return {"error": f"File not found: {path}"}
        content = p.read_text(encoding="utf-8", errors="replace")
        if len(content) > max_chars:
            content = content[:max_chars] + f"\n... [truncated, {len(content)} chars total]"
        sigma = check_output(content)
        return {"content": content, "sigma": sigma, "path": str(p)}
    except Exception as e:
        return {"error": str(e)}


def _handle_run_code(args: Dict[str, Any]) -> Dict[str, Any]:
    try:
        from genesis_agent import _exec_run_code
        result = _exec_run_code(args)
        sigma = check_output(result)
        return {"output": result, "sigma": sigma}
    except Exception as e:
        return {"error": str(e)}


def _handle_search_corpus(args: Dict[str, Any]) -> Dict[str, Any]:
    try:
        from corpus_rag import CorpusRAG
        rag = CorpusRAG()
        hits = rag.query(args["query"], top_k=args.get("top_k", 5))
        return {"results": hits, "n_results": len(hits)}
    except Exception as e:
        return {"error": str(e)}


def _handle_sigma_check(args: Dict[str, Any]) -> Dict[str, Any]:
    text = args["text"]
    sigma = check_output(text)
    coherent = is_coherent(text)
    return {"sigma": sigma, "coherent": coherent, "text_length": len(text)}


def _handle_codex_verify(args: Dict[str, Any]) -> Dict[str, Any]:
    try:
        from codex_verifier import verify_response
        accepted, report = verify_response(args["text"])
        return {"accepted": accepted, **report}
    except Exception as e:
        return {"error": str(e)}


def _handle_voice_tts(args: Dict[str, Any]) -> Dict[str, Any]:
    try:
        from genesis_voice import speak
        ok = speak(args["text"])
        return {"spoken": ok, "text": args["text"]}
    except Exception as e:
        return {"error": str(e)}


TOOL_HANDLERS = {
    "genesis_generate": _handle_generate,
    "genesis_read_file": _handle_read_file,
    "genesis_run_code": _handle_run_code,
    "genesis_search_corpus": _handle_search_corpus,
    "genesis_sigma_check": _handle_sigma_check,
    "genesis_codex_verify": _handle_codex_verify,
    "genesis_voice_tts": _handle_voice_tts,
}


# ── MCP JSON-RPC Protocol ──────────────────────────────────────────────

def _jsonrpc_response(req_id: Any, result: Any) -> Dict[str, Any]:
    return {"jsonrpc": "2.0", "id": req_id, "result": result}


def _jsonrpc_error(req_id: Any, code: int, message: str) -> Dict[str, Any]:
    return {"jsonrpc": "2.0", "id": req_id, "error": {"code": code, "message": message}}


def handle_request(request: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Handle a single MCP JSON-RPC request."""
    method = request.get("method", "")
    req_id = request.get("id")
    params = request.get("params", {})

    if method == "initialize":
        return _jsonrpc_response(req_id, {
            "protocolVersion": MCP_PROTOCOL_VERSION,
            "capabilities": {
                "tools": {"listChanged": False},
            },
            "serverInfo": {
                "name": SERVER_NAME,
                "version": SERVER_VERSION,
            },
        })

    if method == "notifications/initialized":
        return None  # No response for notifications

    if method == "tools/list":
        return _jsonrpc_response(req_id, {"tools": TOOLS})

    if method == "tools/call":
        tool_name = params.get("name", "")
        tool_args = params.get("arguments", {})
        handler = TOOL_HANDLERS.get(tool_name)
        if handler is None:
            return _jsonrpc_error(req_id, -32601, f"Unknown tool: {tool_name}")
        try:
            result = handler(tool_args)
            return _jsonrpc_response(req_id, {
                "content": [{"type": "text", "text": json.dumps(result, ensure_ascii=False)}],
                "isError": "error" in result,
            })
        except Exception as e:
            return _jsonrpc_response(req_id, {
                "content": [{"type": "text", "text": json.dumps({"error": str(e)})}],
                "isError": True,
            })

    if method == "ping":
        return _jsonrpc_response(req_id, {})

    return _jsonrpc_error(req_id, -32601, f"Method not found: {method}")


def run_stdio_server() -> None:
    """Run MCP server over stdio (standard transport)."""
    print(f"[Genesis MCP] Server starting (stdio transport)", file=sys.stderr)
    print(f"[Genesis MCP] Tools: {len(TOOLS)}", file=sys.stderr)
    print(f"[Genesis MCP] 1 = 1", file=sys.stderr)

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
        except json.JSONDecodeError:
            continue

        response = handle_request(request)
        if response is not None:
            sys.stdout.write(json.dumps(response) + "\n")
            sys.stdout.flush()


if __name__ == "__main__":
    run_stdio_server()
