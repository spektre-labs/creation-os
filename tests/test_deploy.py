# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.deploy`` (no real cluster)."""
from __future__ import annotations

from pathlib import Path

from cos.deploy import SigmaDeploy
from cos.sigma_gate import SigmaGate


def test_deploy_local_command() -> None:
    d = SigmaDeploy.deploy_local("/tmp/model", 8420)
    assert "cos serve" in d["command"] and d["port"] == 8420


def test_deploy_docker_quotes_env() -> None:
    d = SigmaDeploy.deploy_docker("cos:latest", 9000, env={"FOO": "bar baz"})
    assert "FOO=" in d["command"] and "-p 9000" in d["command"]


def test_deploy_k8s_and_rollback_strings() -> None:
    d = SigmaDeploy.deploy_k8s("./chart", "prod")
    assert "helm upgrade" in d["command"] and "prod" in d["command"]
    rb = SigmaDeploy.rollback("api", "3")
    assert "rollout undo" in rb["command"]


def test_deploy_airgap_copies(tmp_path: Path) -> None:
    src = tmp_path / "b.txt"
    src.write_text("x", encoding="utf-8")
    dst = tmp_path / "out"
    SigmaDeploy.deploy_airgap(str(src), str(dst))
    assert (dst / "b.txt").exists()


def test_health_after_deploy_invalid_host() -> None:
    r = SigmaDeploy.health_after_deploy("http://127.0.0.1:1", timeout_s=0.5)
    assert r["ok"] is False


def test_smoke_and_blue_green_local() -> None:
    g = SigmaGate()
    bg = SigmaDeploy().blue_green("http://127.0.0.1:1", "http://127.0.0.1:2", g)
    assert bg.get("winner")
    r = SigmaDeploy.smoke_test("http://127.0.0.1:1", g)
    assert r["via"] == "local_gate" and r["ok"] is True
