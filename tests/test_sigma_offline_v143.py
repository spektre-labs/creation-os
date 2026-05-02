# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""v143 σ-offline: air-gap heuristics, INET connect lock, tarball package/deploy."""
from __future__ import annotations

import json
import socket
import subprocess
import sys
import tarfile
from pathlib import Path

from cos.sigma_offline import SigmaOffline, SigmaOfflinePackager


def test_verify_airgap_returns_checks_dict() -> None:
    r = SigmaOffline().verify_airgap()
    assert "airgap_verified" in r
    assert "checks" in r
    for k in ("no_dns", "no_http", "no_env_proxy", "no_telemetry_env"):
        assert k in r["checks"]


def test_enforce_blocks_inet_connect_restore() -> None:
    off = SigmaOffline()
    try:
        off.enforce_airgap()
        sock: socket.socket | None = None
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("127.0.0.1", 65534))
            assert False, "expected ConnectionRefusedError from air-gap patch"
        except ConnectionRefusedError as e:
            assert "air-gap" in str(e).lower() or "blocked" in str(e).lower()
        finally:
            if sock is not None:
                sock.close()
    finally:
        SigmaOffline.restore_socket_connect()


def test_package_deploy_roundtrip(tmp_path: Path) -> None:
    repo = Path(__file__).resolve().parents[1]
    model = tmp_path / "dummy.gguf"
    model.write_bytes(b"GGUF")
    tar_path = tmp_path / "off.tgz"
    dest = tmp_path / "deploy"
    payload = SigmaOfflinePackager(repo_root=repo).package(model, tar_path)
    assert payload.get("airgap") is True
    assert tar_path.is_file()

    try:
        SigmaOfflinePackager(repo_root=repo).deploy(tar_path, dest)
    finally:
        SigmaOffline.restore_socket_connect()

    assert (dest / "start.sh").is_file()
    assert (dest / "model" / "dummy.gguf").is_file()
    assert (dest / "config" / "offline.json").is_file()


def test_package_include_voice_tar_members(tmp_path: Path) -> None:
    repo = Path(__file__).resolve().parents[1]
    model = tmp_path / "m.gguf"
    model.write_bytes(b"x")
    stt = tmp_path / "stt"
    tts = tmp_path / "tts"
    stt.mkdir()
    tts.mkdir()
    (stt / "x.bin").write_bytes(b"1")
    (tts / "y.onnx").write_bytes(b"2")
    out = tmp_path / "v.tgz"
    SigmaOfflinePackager(repo_root=repo).package(
        model,
        out,
        include_voice=True,
        voice_stt=stt,
        voice_tts=tts,
    )
    names: set[str] = set()
    with tarfile.open(out, "r:gz") as tf:
        for m in tf.getmembers():
            names.add(m.name)
    assert any(n.startswith("voice/stt/") for n in names)
    assert any(n.startswith("voice/tts/") for n in names)


def test_cli_chat_offline_json() -> None:
    import os

    env = os.environ.copy()
    root = Path(__file__).resolve().parents[1]
    env["PYTHONPATH"] = str(root / "python") + (
        (":" + env["PYTHONPATH"]) if env.get("PYTHONPATH") else ""
    )
    p = subprocess.run(
        [sys.executable, "-m", "cos", "chat", "--offline", "--model", "p", "--prompt", "hi"],
        env=env,
        check=False,
        capture_output=True,
        text=True,
    )
    assert p.returncode == 0, p.stderr
    data = json.loads(p.stdout)
    assert data.get("network_calls") == 0
    assert data.get("mode") == "offline_chat_lab"
