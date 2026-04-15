#!/usr/bin/env python3
"""
CREATION OS — MLX Metal Server (Genesis API)

OpenAI-compatible HTTP API running entirely on MLX Metal.
Ghost boot, kernel, killswitch, living weights — same pipeline, same silicon.
M4 Air is the server. No cloud. 1 = 1.

Usage:
    python3 creation_os_server.py --model mlx-community/Llama-3.2-3B-Instruct-4bit
    python3 creation_os_server.py --port 8080 --host 0.0.0.0

    curl http://localhost:8080/v1/chat/completions \\
      -H "Content-Type: application/json" \\
      -d '{"model":"genesis","messages":[{"role":"user","content":"Who are you?"}]}'

Environment:
    SPEKTRE_MLX_HOST=127.0.0.1   listen address
    SPEKTRE_MLX_PORT=8080         listen port
    CREATION_OS_MODEL=...         default model path or HF id

1 = 1.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time
import uuid
from http.server import HTTPServer, BaseHTTPRequestHandler
from typing import Any, Dict, List, Optional

from creation_os import creation_os_generate_native, stream_tokens
from creation_os_core import DEFAULT_SYSTEM


class GenesisHandler(BaseHTTPRequestHandler):
    model_path: Optional[str] = None
    adapter_path: Optional[str] = None

    def do_GET(self) -> None:
        if self.path == "/health" or self.path == "/":
            self._json_response(200, {
                "status": "ok",
                "engine": "creation_os_mlx",
                "backend": "metal",
                "invariant": "1=1",
            })
            return
        if self.path == "/v1/models":
            model_id = os.path.basename(self.model_path or "genesis")
            self._json_response(200, {
                "object": "list",
                "data": [{
                    "id": model_id,
                    "object": "model",
                    "created": int(time.time()),
                    "owned_by": "creation_os",
                }],
            })
            return
        self._json_response(404, {"error": "not found"})

    def do_POST(self) -> None:
        if self.path == "/v1/chat/completions":
            self._handle_chat()
            return
        if self.path == "/v1/completions":
            self._handle_completion()
            return
        if self.path == "/v1/agent/run":
            self._handle_agent()
            return
        if self.path == "/v1/proconductor":
            self._handle_proconductor()
            return
        if self.path == "/v1/mesh/query":
            self._handle_mesh_query()
            return
        if self.path == "/v1/corpus/search":
            self._handle_corpus_search()
            return
        if self.path == "/v1/codex/verify":
            self._handle_codex_verify()
            return
        if self.path == "/v1/twins/debate":
            self._handle_twins()
            return
        if self.path == "/v1/self-play":
            self._handle_self_play()
            return
        if self.path == "/v1/sigma/check":
            self._handle_sigma_check()
            return
        self._json_response(404, {"error": "not found"})

    def _read_body(self) -> Dict[str, Any]:
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length) if length > 0 else b"{}"
        return json.loads(raw)

    def _handle_chat(self) -> None:
        body = self._read_body()
        messages: List[Dict[str, str]] = body.get("messages", [])
        max_tokens = int(body.get("max_tokens", 256))
        temperature = float(body.get("temperature", 0.3))
        top_p = float(body.get("top_p", 1.0))
        stream = bool(body.get("stream", False))

        user_prompt = ""
        for msg in reversed(messages):
            if msg.get("role") == "user":
                user_prompt = msg.get("content", "")
                break
        if not user_prompt:
            self._json_response(400, {"error": "no user message"})
            return

        response_format = body.get("response_format", {})
        sigma_json = response_format.get("type") == "sigma_json" or body.get("sigma_json", False)

        if stream:
            self._stream_chat(user_prompt, max_tokens, temperature, top_p, body)
            return

        t0 = time.perf_counter()
        text, receipt = creation_os_generate_native(
            user_prompt,
            model_path=self.model_path,
            adapter_path=self.adapter_path,
            max_tokens=max_tokens,
            temp=temperature,
            top_p=top_p,
        )
        dt = time.perf_counter() - t0
        n_tokens = receipt.get("tokens_generated", 0)

        if sigma_json:
            sigma_val = receipt.get("sigma", 0)
            coherent = receipt.get("coherent", False)
            conf = "high" if sigma_val == 0 and coherent else (
                "medium" if sigma_val <= 2 else (
                    "low" if sigma_val <= 5 else "uncertain"
                )
            )
            structured = {
                "response": text,
                "sigma": sigma_val,
                "confidence": conf,
                "coherent": coherent,
                "sources": [],
                "invariant": "1=1",
            }
            content = json.dumps(structured, ensure_ascii=False)
        else:
            content = text

        resp = {
            "id": f"chatcmpl-{uuid.uuid4().hex[:12]}",
            "object": "chat.completion",
            "created": int(time.time()),
            "model": os.path.basename(self.model_path or "genesis"),
            "choices": [{
                "index": 0,
                "message": {"role": "assistant", "content": content},
                "finish_reason": "stop",
            }],
            "usage": {
                "prompt_tokens": 0,
                "completion_tokens": n_tokens,
                "total_tokens": n_tokens,
            },
            "creation_os": {
                "sigma": receipt.get("sigma", 0),
                "coherent": receipt.get("coherent", False),
                "tier": receipt.get("tier_used", -1),
                "accepted": receipt.get("epistemic_accepted", False),
                "latency_ms": round(dt * 1000, 1),
                "tok_per_sec": round(n_tokens / dt, 1) if dt > 0 else 0,
            },
        }
        self._json_response(200, resp)

    def _stream_chat(
        self,
        prompt: str,
        max_tokens: int,
        temp: float,
        top_p: float,
        body: Dict[str, Any],
    ) -> None:
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()

        model_name = os.path.basename(self.model_path or "genesis")
        chat_id = f"chatcmpl-{uuid.uuid4().hex[:12]}"

        for piece, _tid, sigma in stream_tokens(
            prompt,
            model_path=self.model_path,
            adapter_path=self.adapter_path,
            max_tokens=max_tokens,
            temp=temp,
            top_p=top_p,
        ):
            chunk = {
                "id": chat_id,
                "object": "chat.completion.chunk",
                "created": int(time.time()),
                "model": model_name,
                "choices": [{
                    "index": 0,
                    "delta": {"content": piece},
                    "finish_reason": None,
                }],
            }
            self.wfile.write(f"data: {json.dumps(chunk)}\n\n".encode())
            self.wfile.flush()

        done_chunk = {
            "id": chat_id,
            "object": "chat.completion.chunk",
            "created": int(time.time()),
            "model": model_name,
            "choices": [{"index": 0, "delta": {}, "finish_reason": "stop"}],
        }
        self.wfile.write(f"data: {json.dumps(done_chunk)}\n\n".encode())
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()

    def _handle_agent(self) -> None:
        body = self._read_body()
        task = body.get("task", "")
        max_steps = int(body.get("max_steps", 10))
        if not task:
            self._json_response(400, {"error": "no task"})
            return
        try:
            from genesis_agent import agent_run
            result = agent_run(task, model_path=self.model_path, max_steps=max_steps, verbose=False)
            self._json_response(200, result)
        except Exception as e:
            self._json_response(500, {"error": str(e)})

    def _handle_proconductor(self) -> None:
        body = self._read_body()
        prompt = body.get("prompt", "")
        max_tokens = int(body.get("max_tokens", 256))
        if not prompt:
            self._json_response(400, {"error": "no prompt"})
            return
        try:
            from proconductor import proconductor_generate
            text, receipt = proconductor_generate(
                prompt,
                generator_model=body.get("generator_model", self.model_path),
                verifier_model=body.get("verifier_model"),
                max_tokens=max_tokens,
            )
            self._json_response(200, {"text": text, "receipt": receipt})
        except Exception as e:
            self._json_response(500, {"error": str(e)})

    def _handle_mesh_query(self) -> None:
        body = self._read_body()
        prompt = body.get("prompt", "")
        peers = body.get("peers", [])
        mode = body.get("mode", "parallel")
        if not prompt:
            self._json_response(400, {"error": "no prompt"})
            return
        if not peers:
            self._json_response(400, {"error": "no peers specified"})
            return
        try:
            from genesis_mesh import GenesisMesh
            mesh = GenesisMesh(peers)
            mesh.health_check_all()
            result = mesh.query(prompt, mode=mode, max_tokens=int(body.get("max_tokens", 256)))
            self._json_response(200, result)
        except Exception as e:
            self._json_response(500, {"error": str(e)})

    def _handle_corpus_search(self) -> None:
        body = self._read_body()
        query = body.get("query", "")
        if not query:
            self._json_response(400, {"error": "no query"})
            return
        try:
            from corpus_rag import CorpusRAG
            rag = CorpusRAG()
            hits = rag.query(query, top_k=int(body.get("top_k", 5)))
            self._json_response(200, {"results": hits, "n_results": len(hits)})
        except Exception as e:
            self._json_response(500, {"error": str(e)})

    def _handle_codex_verify(self) -> None:
        body = self._read_body()
        text = body.get("text", "")
        if not text:
            self._json_response(400, {"error": "no text"})
            return
        try:
            from codex_verifier import verify_response
            accepted, report = verify_response(text)
            self._json_response(200, {"accepted": accepted, **report})
        except Exception as e:
            self._json_response(500, {"error": str(e)})

    def _handle_twins(self) -> None:
        body = self._read_body()
        topic = body.get("topic", "")
        if not topic:
            self._json_response(400, {"error": "no topic"})
            return
        try:
            from genesis_twins import twins_debate
            result = twins_debate(
                topic, rounds=int(body.get("rounds", 3)), verbose=False,
            )
            self._json_response(200, result)
        except Exception as e:
            self._json_response(500, {"error": str(e)})

    def _handle_self_play(self) -> None:
        body = self._read_body()
        try:
            from genesis_self_play import self_play_session
            result = self_play_session(
                rounds=int(body.get("rounds", 5)),
                domain=body.get("domain"),
                verbose=False, save=False,
            )
            self._json_response(200, result)
        except Exception as e:
            self._json_response(500, {"error": str(e)})

    def _handle_sigma_check(self) -> None:
        body = self._read_body()
        text = body.get("text", "")
        if not text:
            self._json_response(400, {"error": "no text"})
            return
        sigma = check_output(text)
        coherent = is_coherent(text)
        self._json_response(200, {"sigma": sigma, "coherent": coherent})

    def _handle_completion(self) -> None:
        body = self._read_body()
        prompt = body.get("prompt", "")
        max_tokens = int(body.get("max_tokens", 256))
        temperature = float(body.get("temperature", 0.3))

        text, receipt = creation_os_generate_native(
            prompt,
            model_path=self.model_path,
            adapter_path=self.adapter_path,
            max_tokens=max_tokens,
            temp=temperature,
        )
        resp = {
            "id": f"cmpl-{uuid.uuid4().hex[:12]}",
            "object": "text_completion",
            "created": int(time.time()),
            "model": os.path.basename(self.model_path or "genesis"),
            "choices": [{
                "text": text,
                "index": 0,
                "finish_reason": "stop",
            }],
            "creation_os": {
                "sigma": receipt.get("sigma", 0),
                "coherent": receipt.get("coherent", False),
            },
        }
        self._json_response(200, resp)

    def _json_response(self, code: int, data: Any) -> None:
        body = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt: str, *args: Any) -> None:
        ts = time.strftime("%H:%M:%S")
        sys.stderr.write(f"[{ts}] {fmt % args}\n")


def main() -> None:
    ap = argparse.ArgumentParser(description="Creation OS — MLX Metal Server (Genesis API)")
    ap.add_argument("--model", "-m", default=None)
    ap.add_argument("--adapter", "-a", default=None)
    ap.add_argument("--host", default=os.environ.get("SPEKTRE_MLX_HOST", "127.0.0.1"))
    ap.add_argument("--port", type=int, default=int(os.environ.get("SPEKTRE_MLX_PORT", "8080")))
    args = ap.parse_args()

    GenesisHandler.model_path = args.model or os.environ.get(
        "CREATION_OS_MODEL", "mlx-community/Llama-3.2-3B-Instruct-4bit",
    )
    GenesisHandler.adapter_path = args.adapter

    server = HTTPServer((args.host, args.port), GenesisHandler)
    print("═" * 57, file=sys.stderr)
    print("  CREATION OS — MLX Metal Server (Genesis API)", file=sys.stderr)
    print(f"  Model:  {GenesisHandler.model_path}", file=sys.stderr)
    print(f"  Listen: http://{args.host}:{args.port}", file=sys.stderr)
    print(f"  Endpoints:", file=sys.stderr)
    print(f"    POST /v1/chat/completions  (OpenAI-compatible)", file=sys.stderr)
    print(f"    POST /v1/completions       (legacy)", file=sys.stderr)
    print(f"    GET  /v1/models", file=sys.stderr)
    print(f"    GET  /health", file=sys.stderr)
    print(f"  Backend: MLX Metal (unified memory, zero-copy)", file=sys.stderr)
    print(f"  Creation OS: ghost boot + kernel + killswitch", file=sys.stderr)
    print(f"  1 = 1", file=sys.stderr)
    print("═" * 57, file=sys.stderr)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[Genesis] Shutdown. 1 = 1.", file=sys.stderr)
        server.server_close()


if __name__ == "__main__":
    main()
