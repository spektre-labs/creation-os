# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""``cos serve``: σ-gate as an ASGI service (REST + WebSocket + SSE).

FastAPI + uvicorn. OpenAPI at ``/docs`` and ReDoc at ``/redoc``.

Install::

    pip install 'creation-os[serve]'

Usage::

    cos serve                     # default http://127.0.0.1:8420
    cos serve --host 0.0.0.0 --port 9000

Example::

    curl -s -X POST http://127.0.0.1:8420/v1/score \\
      -H 'Content-Type: application/json' \\
      -d '{"prompt": "What is 2+2?", "response": "4"}'

JWT and auth are deployment concerns; add middleware in your stack. For claim
discipline on benchmarks see ``docs/CLAIM_DISCIPLINE.md`` (``GET /v1/evidence``).
"""
from __future__ import annotations

import asyncio
import json
import time
from typing import Any, List, Optional

try:
    from fastapi import FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect
    from fastapi.middleware.cors import CORSMiddleware
    from fastapi.responses import StreamingResponse
    from pydantic import BaseModel, ConfigDict

    HAS_FASTAPI = True
except ImportError:  # pragma: no cover
    HAS_FASTAPI = False
    BaseModel = object  # type: ignore[misc, assignment]

from cos import __version__
from cos.calibrate import SigmaCalibrator
from cos.pipeline import Pipeline
from cos.sigma_gate import SigmaGate
from cos.stream import SigmaStream


class ScoreRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    prompt: str
    response: str
    calibrate: bool = False


class ScoreResponse(BaseModel):
    model_config = ConfigDict(extra="forbid")

    sigma: float
    verdict: str
    calibrated: bool = False
    elapsed_ms: float = 0.0


class PipeRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    prompt: str
    response: str


class PipeResponse(BaseModel):
    model_config = ConfigDict(extra="forbid")

    text: Optional[str]
    sigma: float
    verdict: str
    reason: Optional[str] = None
    attempt: int = 1
    elapsed_ms: float = 0.0


class StreamRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    prompt: str
    tokens: List[str]


class BatchScoreRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    items: List[ScoreRequest]


class BatchScoreResponse(BaseModel):
    model_config = ConfigDict(extra="forbid")

    results: List[ScoreResponse]
    total_ms: float


class HealthResponse(BaseModel):
    model_config = ConfigDict(extra="forbid")

    status: str
    version: str
    uptime_s: float
    requests_total: int
    sigma_gate: str = "operational"


def create_app() -> Any:
    if not HAS_FASTAPI:  # pragma: no cover
        raise ImportError("pip install 'creation-os[serve]'")

    def _optional_bearer_auth(authorization: Optional[str]) -> None:
        import os

        key = (os.environ.get("CREATION_OS_API_KEY") or "").strip()
        if not key:
            return
        if not authorization or not authorization.startswith("Bearer "):
            raise HTTPException(status_code=401, detail="missing bearer token")
        got = authorization[7:].strip()
        if got != key:
            raise HTTPException(status_code=403, detail="invalid bearer token")

    app = FastAPI(
        title="Creation OS — σ-gate API",
        description=(
            "Hallucination firewall for LLMs — score prompt–response pairs "
            "(no model required)."
        ),
        version=__version__,
        docs_url="/docs",
        redoc_url="/redoc",
    )

    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_methods=["*"],
        allow_headers=["*"],
    )

    gate = SigmaGate()
    pipeline = Pipeline(gate=gate)
    stream_engine = SigmaStream(gate=gate)
    calibrator = SigmaCalibrator()
    start_time = time.monotonic()
    request_count = 0

    @app.get("/health", response_model=HealthResponse)
    async def health(request: Request) -> HealthResponse:
        _optional_bearer_auth(request.headers.get("Authorization"))
        return HealthResponse(
            status="ok",
            version=__version__,
            uptime_s=round(time.monotonic() - start_time, 1),
            requests_total=request_count,
        )

    @app.get("/")
    async def root() -> dict[str, Any]:
        return {"service": "creation-os", "version": __version__, "docs": "/docs"}

    @app.post("/v1/score", response_model=ScoreResponse)
    async def score(req: ScoreRequest, request: Request) -> ScoreResponse:
        nonlocal request_count
        _optional_bearer_auth(request.headers.get("Authorization"))
        request_count += 1
        t0 = time.monotonic()
        sigma, verdict = gate.score(req.prompt, req.response)

        calibrated = False
        out_sigma = float(sigma)
        if req.calibrate and calibrator.fitted:
            out_sigma = float(calibrator.calibrate(out_sigma))
            calibrated = True

        elapsed = (time.monotonic() - t0) * 1000.0
        return ScoreResponse(
            sigma=round(out_sigma, 6),
            verdict=str(verdict),
            calibrated=calibrated,
            elapsed_ms=round(elapsed, 2),
        )

    @app.post("/v1/pipe", response_model=PipeResponse)
    async def pipe(req: PipeRequest, request: Request) -> PipeResponse:
        nonlocal request_count
        _optional_bearer_auth(request.headers.get("Authorization"))
        request_count += 1
        t0 = time.monotonic()
        result = pipeline.score(req.prompt, req.response)
        elapsed = (time.monotonic() - t0) * 1000.0
        return PipeResponse(
            text=result.text,
            sigma=round(float(result.sigma), 6),
            verdict=str(result.verdict),
            reason=result.reason,
            attempt=int(result.attempt),
            elapsed_ms=round(elapsed, 2),
        )

    @app.post("/v1/batch", response_model=BatchScoreResponse)
    async def batch(req: BatchScoreRequest, request: Request) -> BatchScoreResponse:
        nonlocal request_count
        _optional_bearer_auth(request.headers.get("Authorization"))
        request_count += len(req.items)
        t0 = time.monotonic()
        results: List[ScoreResponse] = []
        for item in req.items:
            sigma, verdict = gate.score(item.prompt, item.response)
            calibrated = False
            out_sigma = float(sigma)
            if item.calibrate and calibrator.fitted:
                out_sigma = float(calibrator.calibrate(out_sigma))
                calibrated = True
            results.append(
                ScoreResponse(
                    sigma=round(out_sigma, 6),
                    verdict=str(verdict),
                    calibrated=calibrated,
                )
            )
        total_ms = (time.monotonic() - t0) * 1000.0
        return BatchScoreResponse(results=results, total_ms=round(total_ms, 2))

    @app.post("/v1/stream")
    async def stream_score(req: StreamRequest, request: Request) -> StreamingResponse:
        nonlocal request_count
        _optional_bearer_auth(request.headers.get("Authorization"))
        request_count += 1

        async def event_generator():
            for event in stream_engine.score_stream(req.prompt, req.tokens):
                payload = {
                    "token": event.token,
                    "index": event.index,
                    "sigma": round(event.sigma, 4),
                    "verdict": event.verdict,
                }
                yield f"data: {json.dumps(payload)}\n\n"
                await asyncio.sleep(0)

            full = " ".join(req.tokens)
            sigma, verdict = gate.score(req.prompt, full)
            final = json.dumps(
                {
                    "token": None,
                    "index": len(req.tokens),
                    "sigma": round(float(sigma), 4),
                    "verdict": str(verdict),
                    "is_final": True,
                }
            )
            yield f"data: {final}\n\n"

        return StreamingResponse(event_generator(), media_type="text/event-stream")

    @app.websocket("/v1/ws")
    async def websocket_sigma(ws: WebSocket) -> None:
        nonlocal request_count
        await ws.accept()
        try:
            while True:
                data = await ws.receive_json()
                request_count += 1
                action = data.get("action", "score")

                if action == "score":
                    prompt = str(data.get("prompt", ""))
                    response = str(data.get("response", ""))
                    sigma, verdict = gate.score(prompt, response)
                    await ws.send_json(
                        {"sigma": round(float(sigma), 6), "verdict": str(verdict)}
                    )

                elif action == "stream":
                    prompt = str(data.get("prompt", ""))
                    tokens = list(data.get("tokens") or [])
                    for event in stream_engine.score_stream(prompt, tokens):
                        await ws.send_json(
                            {
                                "token": event.token,
                                "index": event.index,
                                "sigma": round(event.sigma, 4),
                                "verdict": event.verdict,
                            }
                        )
                    full = " ".join(tokens)
                    sigma, verdict = gate.score(prompt, full)
                    await ws.send_json(
                        {
                            "sigma": round(float(sigma), 4),
                            "verdict": str(verdict),
                            "is_final": True,
                        }
                    )

                elif action == "ping":
                    await ws.send_json({"pong": True})

        except WebSocketDisconnect:
            pass

    @app.get("/v1/stats")
    async def stats(request: Request) -> dict[str, Any]:
        _optional_bearer_auth(request.headers.get("Authorization"))
        return {
            "pipeline": pipeline.stats.to_dict(),
            "uptime_s": round(time.monotonic() - start_time, 1),
            "requests_total": request_count,
        }

    @app.get("/v1/evidence")
    async def evidence() -> dict[str, Any]:
        return {
            "status": "NOT_AGI_ACHIEVED",
            "metrics_embedded_here": False,
            "read_path": [
                "docs/CLAIM_DISCIPLINE.md",
                "docs/RESEARCH_AND_THESIS_ARCHITECTURE.md",
                "README.md#measured",
            ],
            "note": (
                "Archived benchmark JSON lives under benchmarks/; "
                "do not hardcode headline AUROC in the API."
            ),
        }

    return app


def run_server(host: str = "127.0.0.1", port: int = 8420) -> None:
    if not HAS_FASTAPI:  # pragma: no cover
        print("ERROR: pip install 'creation-os[serve]'", flush=True)
        return
    try:
        import uvicorn
    except ImportError:  # pragma: no cover
        print("ERROR: pip install uvicorn", flush=True)
        return

    app = create_app()
    print(f"Creation OS σ-gate server on http://{host}:{port}", flush=True)
    print(f"  Docs:    http://{host}:{port}/docs", flush=True)
    print(f"  Health:  http://{host}:{port}/health", flush=True)
    uvicorn.run(app, host=host, port=port, log_level="info")


__all__ = [
    "BatchScoreRequest",
    "BatchScoreResponse",
    "HAS_FASTAPI",
    "HealthResponse",
    "PipeRequest",
    "PipeResponse",
    "ScoreRequest",
    "ScoreResponse",
    "StreamRequest",
    "create_app",
    "run_server",
]
