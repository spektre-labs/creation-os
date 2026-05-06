# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Cross-module integration smoke tests (v253)."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

import pytest

_REPO = Path(__file__).resolve().parents[1]


def _env() -> dict[str, str]:
    env = os.environ.copy()
    env["PYTHONPATH"] = str(_REPO / "python") + os.pathsep + env.get("PYTHONPATH", "")
    return env


def test_full_pipeline_score() -> None:
    from cos import SigmaGate

    gate = SigmaGate()
    sigma, verdict = gate.score("What is 2+2?", "4")
    assert verdict in ("ACCEPT", "RETHINK", "ABSTAIN")
    assert 0 <= sigma <= 1


def test_graph_to_rag() -> None:
    from cos.graph import SigmaGraph

    kg = SigmaGraph()
    kg.add("A", "rel", "B", sigma=0.1)
    results = kg.query("A")
    assert len(results) >= 1


def test_decorator_to_gate() -> None:
    from cos.integrations.decorator import sigma_gated

    @sigma_gated
    def fake(p: str) -> str:
        del p
        return "hello"

    r = fake("test")
    assert r.verdict in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_cascade_levels() -> None:
    from cos import SigmaGate

    gate = SigmaGate()
    result = gate.score_cascade("test", "response")
    assert "L1_entropy" in result["levels"]


def test_cli_smoke() -> None:
    r = subprocess.run(
        [sys.executable, "-m", "cos.cli", "score", "--prompt", "test", "--response", "hello"],
        capture_output=True,
        text=True,
        cwd=str(_REPO),
        env=_env(),
        check=False,
    )
    assert r.returncode in (0, 2)


def test_import_all_modules() -> None:
    from cos import SigmaGate
    from cos.continual import SigmaContinual
    from cos.formal import SigmaFormal
    from cos.graph import SigmaGraph
    from cos.integrations.decorator import sigma_gated

    assert SigmaGate is not None
    assert sigma_gated is not None
    assert SigmaGraph is not None
    assert SigmaFormal is not None
    assert SigmaContinual is not None


def test_serve_identity_and_trace_headers_optional() -> None:
    pytest.importorskip("fastapi")
    from fastapi.testclient import TestClient

    from cos.serve import create_app

    client = TestClient(create_app())
    r = client.get("/v1/identity")
    assert r.status_code == 200
    body = r.json()
    assert "codex" in body
    assert "system_prompt_preview" in body

    r2 = client.post(
        "/v1/score",
        json={
            "prompt": "x",
            "response": '{"answer": 1}',
            "json_schema": {
                "type": "object",
                "required": ["answer"],
                "properties": {"answer": {"type": "integer"}},
            },
        },
    )
    assert r2.status_code == 200
    assert r2.headers.get("X-Trace-Id")
    assert r2.json().get("structure_ok") is True
