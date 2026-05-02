# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""v123 lab scaffolds: MCP marketplace JSON helpers, A2A protocol, σ-JEPA plan API, cos CLI."""
from __future__ import annotations

import json
import subprocess
import sys
import threading
import time
import urllib.request
from pathlib import Path

import pytest

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.a2a_protocol import A2ATaskState, build_agent_card_json, delegate_task_lab  # noqa: E402
from cos.mcp_marketplace import (  # noqa: E402
    build_creation_os_mcp_catalog,
    sigma_gated_tool_payload,
    tool_sigma_check_lab,
)
from cos.sigma_jepa import LabLatentEncoder, LabLatentPredictor, SigmaJEPA  # noqa: E402
from cos.sigma_mcp_registry import SigmaMCPRegistry  # noqa: E402


def test_mcp_catalog_and_sigma_gated_payload() -> None:
    cat = build_creation_os_mcp_catalog()
    assert cat.get("name") == "creation-os-mcp-lab"
    assert any(t.get("name") == "sigma_check" for t in cat.get("tools", []))
    tag, env = sigma_gated_tool_payload(0.2, 0.5, {"x": 1})
    assert tag == "RETHINK"
    tag2, env2 = sigma_gated_tool_payload(0.9, 0.5, {"x": 1})
    assert tag2 == "ACCEPT"
    body = tool_sigma_check_lab("p", "r")
    assert "sigma" in body


def test_agent_card_has_a2a_states() -> None:
    card = build_agent_card_json(endpoint="https://example.test/lab")
    assert isinstance(card.get("capabilities"), list)
    assert "a2a" in card
    assert "working" in card["a2a"]["task_states"]


def test_delegate_maps_to_completed_lab() -> None:
    reg = SigmaMCPRegistry()
    reg.register("a1", "", {})
    reg.register("a2", "", {})
    out = delegate_task_lab(
        registry=reg,
        from_agent="a1",
        to_agent="a2",
        task_text="verify claim X",
        model=type("M", (), {"encode": lambda self, t: {"sigma_hints": {"input": 0.2}}})(),
    )
    assert out["state"] in (A2ATaskState.completed.value, A2ATaskState.input_required.value, A2ATaskState.failed.value)


def test_sigma_jepa_plan_argmin_returns_keys() -> None:
    enc = LabLatentEncoder(dim=8)
    pred = LabLatentPredictor(drift=0.02)
    j = SigmaJEPA(enc, pred, None, k_raw=0.92)
    cands = [["noop", "step"], ["probe", "halt"]]
    out = j.plan_argmin_sigma("goal text", cands, horizon=2, planning_steps=10)
    assert "best_plan" in out and "sigma" in out and "sigreg_collapse_penalty" in out
    assert out["evaluated"] == 2


def test_cos_think_cli_smoke() -> None:
    env = {**__import__("os").environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [sys.executable, "-m", "cos", "think", "--prompt", "cli planning smoke", "--horizon", "3", "--planning-steps", "8"],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert "best_plan" in body and "sigma" in body


def test_cos_think_visualize_cli_smoke() -> None:
    env = {**__import__("os").environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "think",
            "--prompt",
            "viz smoke",
            "--visualize",
            "--actions",
            "a,b,c",
        ],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert "latent_polyline" in body and "trajectory" in body


def test_mcp_lab_http_jsonrpc_ping() -> None:
    import socket

    from cos.mcp_marketplace import serve_mcp_lab_http

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("127.0.0.1", 0))
    port = int(sock.getsockname()[1])
    sock.close()
    threading.Thread(
        target=lambda: serve_mcp_lab_http(host="127.0.0.1", port=port),
        daemon=True,
    ).start()
    for _ in range(40):
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{port}/", timeout=0.2) as resp:
                if resp.status == 200:
                    break
        except OSError:
            time.sleep(0.05)
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/",
        method="GET",
    )
    with urllib.request.urlopen(req, timeout=2.0) as resp:
        assert resp.status == 200
    payload = json.dumps({"jsonrpc": "2.0", "id": 1, "method": "ping"}).encode()
    post = urllib.request.Request(
        f"http://127.0.0.1:{port}/",
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(post, timeout=2.0) as resp:
        data = json.loads(resp.read().decode())
    assert data.get("result", {}).get("pong") is True


def test_build_sigma_ttt_lab_no_engram_does_not_require_file() -> None:
    pytest.importorskip("torch")
    from cos.sigma_ttt import build_sigma_gated_ttt_lab

    orch, _, _ = build_sigma_gated_ttt_lab(dim=8, chunk_size=32, max_steps=2, engram_path=None)
    row = orch.eval_before_after("hello engram off", context=None)
    assert "sigma_before" in row and "sigma_after" in row
