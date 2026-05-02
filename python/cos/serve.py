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
import os
import time
import uuid
from typing import Any, Dict, List, Optional

try:
    from fastapi import FastAPI, HTTPException, Query, Request, Response, WebSocket, WebSocketDisconnect
    from fastapi.middleware.cors import CORSMiddleware
    from fastapi.responses import PlainTextResponse, StreamingResponse
    from pydantic import BaseModel, ConfigDict

    HAS_FASTAPI = True
except ImportError:  # pragma: no cover
    HAS_FASTAPI = False
    BaseModel = object  # type: ignore[misc, assignment]

    def ConfigDict(**_kwargs: Any) -> dict[str, Any]:  # type: ignore[misc]
        """Stub when FastAPI/Pydantic are not installed (default-install import surface)."""
        return {}

from cos import __version__
from cos.calibrate import SigmaCalibrator
from cos.codex import SigmaCodex
from cos.compliance import SigmaCompliance
from cos.explain import SigmaExplain
from cos.feedback import SigmaFeedback
from cos.metric import SigmaMetrics
from cos.persona import SigmaPersona
from cos.pipeline import Pipeline
from cos.plugin import SigmaPlugin
from cos.report import SigmaReport
from cos.serving import SigmaServing
from cos.sigma_gate import SigmaGate
from cos.stream import SigmaStream
from cos.struct import SigmaStruct
from cos.trace import SigmaTrace
from cos.webhook import SigmaWebhook
from cos.workflow import SigmaWorkflow


class ScoreRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    prompt: str
    response: str
    calibrate: bool = False
    json_schema: Optional[Dict[str, Any]] = None


class ScoreResponse(BaseModel):
    model_config = ConfigDict(extra="forbid")

    sigma: float
    verdict: str
    calibrated: bool = False
    elapsed_ms: float = 0.0
    structure_ok: Optional[bool] = None
    combined_ok: Optional[bool] = None
    structure_errors: Optional[List[str]] = None


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


class FeedbackRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    verdict: str
    user_reaction: str
    kind: Optional[str] = None


class ExplainRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    prompt: str
    response: str
    sigma: Optional[float] = None
    verdict: Optional[str] = None
    include_counterfactual: bool = False


class WebhookCreateRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    event: str
    url: str
    payload_template: str = ""
    threshold: float = 0.35


class WorkflowRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    input: str = ""
    demo: str = "linear"


class ReportRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    kind: str = "mtier"
    format: str = "markdown"
    bench_results: Optional[Dict[str, Any]] = None
    evidence: Optional[Dict[str, Any]] = None
    compliance_data: Optional[Dict[str, Any]] = None
    repo_url: str = "https://github.com/spektre-labs/creation-os"
    from_version: str = "0.0.0"
    to_version: str = "0.0.0"


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

    metrics_hub = SigmaMetrics()

    @app.middleware("http")
    async def cos_session_scope(request: Request, call_next: Any) -> Any:
        sid = request.headers.get("X-Creation-OS-Session") or str(uuid.uuid4())
        request.state.cos_session_id = sid
        response = await call_next(request)
        response.headers["X-Creation-OS-Session"] = str(sid)
        return response

    gate = SigmaGate()
    sigma_serving = SigmaServing()
    batch_throughput = sigma_serving.throughput_monitor(120.0, name="v1_batch")
    feedback_engine = SigmaFeedback()
    explainer = SigmaExplain(gate=gate)
    compliance_engine = SigmaCompliance(gate=gate)
    webhook_hub = SigmaWebhook()
    persona_engine = SigmaPersona()
    plugin_hub = SigmaPlugin(gate=gate)
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

    @app.get("/v1/identity")
    async def identity(request: Request) -> dict[str, Any]:
        _optional_bearer_auth(request.headers.get("Authorization"))
        cx = SigmaCodex()
        return {
            "codex": cx.capability_manifest(),
            "system_prompt_preview": cx.system_prompt()[:2000],
        }

    @app.post("/v1/score", response_model=ScoreResponse)
    async def score(req: ScoreRequest, request: Request, response: Response) -> ScoreResponse:
        nonlocal request_count
        _optional_bearer_auth(request.headers.get("Authorization"))
        request_count += 1
        t0 = time.monotonic()
        tr = SigmaTrace()
        response.headers["X-Trace-Id"] = tr.trace_id
        with tr.span("sigma_gate") as span_rec:
            sigma, verdict = gate.score(req.prompt, req.response)
            span_rec["sigma"] = float(sigma)
            span_rec["verdict"] = str(verdict)

        struct_ok: Optional[bool] = None
        combined_ok: Optional[bool] = None
        struct_err: Optional[List[str]] = None
        if req.json_schema:
            comb = SigmaStruct().combined_score(req.response, req.json_schema, req.prompt, gate)
            struct_ok = bool(comb.get("structure_ok"))
            combined_ok = bool(comb.get("ok"))
            struct_err = list(comb.get("structure_errors") or [])
            with tr.span("structure_validate") as srec:
                srec["sigma"] = float(comb.get("sigma", sigma))
                srec["verdict"] = str(comb.get("verdict", verdict))
                srec["metadata"] = {"combined_ok": combined_ok}

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
            structure_ok=struct_ok,
            combined_ok=combined_ok,
            structure_errors=struct_err,
        )

    @app.post("/v1/chat/completions")
    async def chat_completions_proxy(request: Request) -> Any:
        """σ-aware chat proxy: OpenAI client to backend, score assistant reply (non-streaming)."""
        nonlocal request_count
        _optional_bearer_auth(request.headers.get("Authorization"))
        request_count += 1

        try:
            body: dict[str, Any] = await request.json()
        except Exception as e:
            raise HTTPException(status_code=400, detail=f"invalid JSON body: {e}") from e

        base = (
            os.environ.get("COS_BACKEND")
            or os.environ.get("CREATION_OS_LLM_BASE_URL")
            or os.environ.get("CREATION_OS_CHAT_ENDPOINT")
            or os.environ.get("COS_ENDPOINT")
            or "http://127.0.0.1:8000/v1"
        ).rstrip("/")

        def _run() -> dict[str, Any]:
            from cos.chat import SigmaChat

            chat = SigmaChat(endpoint=base, preserve_thinking=True)
            return chat.complete_from_openai_request(body)

        try:
            result = await asyncio.to_thread(_run)
        except ImportError as e:
            raise HTTPException(status_code=500, detail=str(e)) from e

        err = result.get("error") if isinstance(result, dict) else None
        if err:
            code = 502
            if "messages required" in str(err) or "stream=true" in str(err):
                code = 400
            raise HTTPException(status_code=code, detail=str(err))

        return result

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
        rows = [{"prompt": item.prompt, "response": item.response} for item in req.items]
        scored = sigma_serving.batch_score(rows, gate, monitor=batch_throughput)
        results: List[ScoreResponse] = []
        for item, rec in zip(req.items, scored):
            calibrated = False
            out_sigma = float(rec["sigma"])
            verdict = str(rec["verdict"])
            if item.calibrate and calibrator.fitted:
                out_sigma = float(calibrator.calibrate(out_sigma))
                calibrated = True
            results.append(
                ScoreResponse(
                    sigma=round(out_sigma, 6),
                    verdict=verdict,
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

    @app.post("/v1/feedback")
    async def feedback_v1(req: FeedbackRequest, request: Request) -> dict[str, Any]:
        nonlocal request_count
        _optional_bearer_auth(request.headers.get("Authorization"))
        request_count += 1
        return feedback_engine.collect(req.verdict, req.user_reaction, kind=req.kind)

    @app.post("/v1/explain")
    async def explain_v1(req: ExplainRequest, request: Request) -> dict[str, Any]:
        nonlocal request_count
        _optional_bearer_auth(request.headers.get("Authorization"))
        request_count += 1
        if req.sigma is not None and req.verdict is not None:
            s, v = float(req.sigma), str(req.verdict)
        else:
            s, v = gate.score(req.prompt, req.response)
        out: dict[str, Any] = {"explanation": explainer.explain(req.prompt, req.response, s, v)}
        if req.include_counterfactual:
            out["counterfactual"] = explainer.counterfactual(req.prompt, req.response)
        return out

    @app.get("/v1/compliance")
    async def compliance_v1(request: Request) -> dict[str, Any]:
        _optional_bearer_auth(request.headers.get("Authorization"))
        return compliance_engine.full_report(
            {
                "sample_pairs": [{"prompt": "ping", "response": "pong"}],
                "bench_results": {},
                "audit_log": [],
                "model": {"training_data_summary": "operator_supplied"},
                "probes": [{"name": "entropy"}],
            },
        )

    @app.get("/v1/plugins")
    async def plugins_v1(request: Request) -> dict[str, Any]:
        _optional_bearer_auth(request.headers.get("Authorization"))
        return {"plugins": plugin_hub.list_plugins()}

    @app.get("/v1/webhooks")
    async def webhooks_list(request: Request) -> dict[str, Any]:
        _optional_bearer_auth(request.headers.get("Authorization"))
        return {"subscriptions": webhook_hub.subscriptions()}

    @app.post("/v1/webhooks")
    async def webhooks_create(req: WebhookCreateRequest, request: Request) -> dict[str, Any]:
        nonlocal request_count
        _optional_bearer_auth(request.headers.get("Authorization"))
        request_count += 1
        ev = str(req.event).lower().strip()
        if ev == "abstain":
            sid = webhook_hub.on_abstain(req.url, req.payload_template or "{}")
        elif ev == "drift":
            sid = webhook_hub.on_drift(req.url, req.threshold)
        elif ev == "incident":
            sid = webhook_hub.on_incident(req.url)
        elif ev in ("version", "version_change"):
            sid = webhook_hub.on_version_change(req.url)
        else:
            raise HTTPException(status_code=400, detail="unknown event type")
        return {"id": sid}

    @app.delete("/v1/webhooks/{sub_id}")
    async def webhooks_delete(sub_id: str, request: Request) -> dict[str, Any]:
        _optional_bearer_auth(request.headers.get("Authorization"))
        if not webhook_hub.delete_subscription(sub_id):
            raise HTTPException(status_code=404, detail="subscription not found")
        return {"deleted": True}

    @app.get("/metrics")
    async def prometheus_metrics(request: Request) -> PlainTextResponse:
        _optional_bearer_auth(request.headers.get("Authorization"))
        return PlainTextResponse(
            metrics_hub.export_prometheus(),
            media_type="text/plain; version=0.0.4; charset=utf-8",
        )

    @app.post("/v1/workflow")
    async def workflow_v1(req: WorkflowRequest, request: Request) -> dict[str, Any]:
        nonlocal request_count
        _optional_bearer_auth(request.headers.get("Authorization"))
        request_count += 1
        wf_exec = SigmaWorkflow(metrics=metrics_hub)

        def good(st: Dict[str, Any]) -> Dict[str, Any]:
            o = dict(st)
            o["prompt"] = "What is 2+2?"
            o["response"] = "4"
            return o

        demo = str(req.demo).lower().strip()
        if demo == "linear":
            spec = wf_exec.define(
                [
                    {
                        "name": "step1",
                        "fn": good,
                        "sigma_threshold": 1.0,
                        "max_retries": 1,
                        "step_cost_eur": 0.0001,
                    },
                    {
                        "name": "step2",
                        "fn": good,
                        "sigma_threshold": 1.0,
                        "max_retries": 1,
                        "step_cost_eur": 0.0001,
                    },
                ]
            )
        elif demo == "stop":

            def bad(st: Dict[str, Any]) -> Dict[str, Any]:
                o = dict(st)
                o["prompt"] = "q"
                o["response"] = ""
                return o

            spec = wf_exec.define(
                [{"name": "only", "fn": bad, "sigma_threshold": 1.0, "max_retries": 0}],
            )
        else:
            spec = wf_exec.define(
                [{"name": "once", "fn": good, "sigma_threshold": 1.0, "max_retries": 1}],
            )
        out = wf_exec.execute(spec, req.input)
        return {
            "demo": req.demo,
            "ok": out["ok"],
            "trace_id": out["trace_id"],
            "step_results": out["step_results"],
            "stopped": out["stopped"],
            "cost_eur": out["cost_eur"],
            "checkpoints": out["checkpoints"],
            "session_id": getattr(request.state, "cos_session_id", None),
        }

    @app.post("/v1/report")
    async def report_v1(req: ReportRequest, request: Request) -> dict[str, Any]:
        nonlocal request_count
        _optional_bearer_auth(request.headers.get("Authorization"))
        request_count += 1
        rep = SigmaReport()
        kind = str(req.kind).lower().strip()
        if kind == "mtier":
            body = rep.generate_mtier_table(req.bench_results or {})
        elif kind == "ladder":
            body = rep.generate_evidence_ladder(req.evidence or {})
        elif kind == "reddit":
            mt = rep.generate_mtier_table(req.bench_results or {"lab_placeholder": {}})
            ev = rep.generate_evidence_ladder(
                req.evidence
                or {
                    "positives": ["Archived harness JSON cited per CLAIM_DISCIPLINE"],
                    "negatives": ["Not a peer-reviewed leaderboard row"],
                }
            )
            body = rep.generate_reddit_post(mt, ev, req.repo_url)
        elif kind == "compliance":
            body = rep.generate_compliance_report(req.compliance_data or {"system": {}, "mitigations": {}, "data_governance": {}})
        elif kind == "changelog":
            body = rep.generate_changelog(req.from_version, req.to_version)
        else:
            raise HTTPException(status_code=400, detail="unknown report kind")
        try:
            rendered = rep.format_body(body, req.format)
        except ValueError as e:
            raise HTTPException(status_code=400, detail=str(e)) from e
        return {
            "kind": kind,
            "format": req.format,
            "body": rendered,
            "session_id": getattr(request.state, "cos_session_id", None),
        }

    @app.get("/v1/stats")
    async def stats(request: Request) -> dict[str, Any]:
        _optional_bearer_auth(request.headers.get("Authorization"))
        return {
            "pipeline": pipeline.stats.to_dict(),
            "uptime_s": round(time.monotonic() - start_time, 1),
            "requests_total": request_count,
            "sigma_serving": {"batch_throughput": batch_throughput.snapshot()},
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
    "ExplainRequest",
    "FeedbackRequest",
    "HAS_FASTAPI",
    "HealthResponse",
    "PipeRequest",
    "PipeResponse",
    "ReportRequest",
    "ScoreRequest",
    "ScoreResponse",
    "StreamRequest",
    "WebhookCreateRequest",
    "WorkflowRequest",
    "create_app",
    "run_server",
]
