# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-offline — host posture checks and packaging helpers for **air-gapped** workflows.

Heuristics (DNS reachability, TCP probes, env vars) are **not a CMMC/FedRAMP attestation**;
operators still owe network isolation review. Does not modify ``sigma_gate.h``.

See ``docs/CLAIM_DISCIPLINE.md`` before conflating this module with certified compliance.
"""
from __future__ import annotations

import io
import json
import os
import socket
import tarfile
from pathlib import Path
from typing import Any, Callable, Dict, Optional

# Preserve original ``connect`` for optional restore (tests / REPL).
_ORIG_SOCKET_CONNECT: Optional[Callable[..., None]] = None


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


class SigmaOffline:
    """Best-effort air-gap posture signals; optional INET-only ``connect`` lock-down."""

    def __init__(self) -> None:
        self.verified = False

    def verify_airgap(self) -> Dict[str, Any]:
        checks = {
            "no_dns": self.check_no_dns(),
            "no_http": self.check_no_http(),
            "no_env_proxy": self.check_no_proxy_env(),
            "no_telemetry_env": self.check_no_telemetry(),
        }
        self.verified = bool(all(checks.values()))
        return {"airgap_verified": self.verified, "checks": checks}

    def check_no_dns(self) -> bool:
        """Return True when resolver cannot resolve a public test name (heuristic)."""
        try:
            socket.getaddrinfo("dns.google", 443, socket.AF_INET, socket.SOCK_STREAM)
            return False
        except (socket.gaierror, OSError):
            return True

    def check_no_http(self) -> bool:
        """Return True when outbound TCP to a well-known resolver fails quickly."""
        try:
            s = socket.create_connection(("1.1.1.1", 80), timeout=2.0)
            s.close()
            return False
        except (ConnectionRefusedError, TimeoutError, OSError):
            return True

    def check_no_proxy_env(self) -> bool:
        proxy_vars = (
            "HTTP_PROXY",
            "HTTPS_PROXY",
            "ALL_PROXY",
            "http_proxy",
            "https_proxy",
            "all_proxy",
        )
        return not any(str(os.environ.get(v, "")).strip() for v in proxy_vars)

    def check_no_telemetry(self) -> bool:
        telemetry_vars = (
            "SENTRY_DSN",
            "OTEL_EXPORTER_OTLP_ENDPOINT",
            "OTEL_EXPORTER",
            "DD_API_KEY",
            "NEW_RELIC_LICENSE_KEY",
            "NEW_RELIC_KEY",
        )
        return not any(str(os.environ.get(v, "")).strip() for v in telemetry_vars)

    def enforce_airgap(self) -> Dict[str, Any]:
        """
        Block IPv4/IPv6 ``socket.connect`` for this process (Unix domain sockets still allowed).

        Sets ``COS_AIRGAP=1`` and strips common proxy env vars from ``os.environ``.
        """
        global _ORIG_SOCKET_CONNECT
        if _ORIG_SOCKET_CONNECT is None:
            _ORIG_SOCKET_CONNECT = socket.socket.connect  # type: ignore[assignment]

        def _blocked_connect(self_socket: socket.socket, address: Any) -> None:
            fam = getattr(self_socket, "family", None)
            if fam in (socket.AF_INET, socket.AF_INET6):
                raise ConnectionRefusedError(
                    f"cos offline: INET connect blocked (air-gap). Attempted: {address!r}"
                )
            assert _ORIG_SOCKET_CONNECT is not None
            return _ORIG_SOCKET_CONNECT(self_socket, address)

        socket.socket.connect = _blocked_connect  # type: ignore[assignment]

        os.environ["COS_AIRGAP"] = "1"
        os.environ["COS_OFFLINE"] = "1"
        for k in ("HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "http_proxy", "https_proxy", "all_proxy"):
            os.environ.pop(k, None)
        return {"enforced": True, "mode": "inet_blocked", "note": "Unix-domain sockets are not blocked."}

    @classmethod
    def restore_socket_connect(cls) -> None:
        """Undo :meth:`enforce_airgap` connect monkey-patch (testing / tooling)."""
        global _ORIG_SOCKET_CONNECT
        if _ORIG_SOCKET_CONNECT is not None:
            socket.socket.connect = _ORIG_SOCKET_CONNECT  # type: ignore[assignment]


class SigmaOfflinePackager:
    """Build a portable tarball (code + model + optional probe + config + launcher)."""

    def __init__(self, repo_root: Optional[Path] = None) -> None:
        self.repo_root = Path(repo_root) if repo_root is not None else _repo_root()

    def package(
        self,
        model_path: str | Path,
        output_path: str | Path = "creation-os-offline.tar.gz",
        *,
        include_voice: bool = False,
        voice_stt: Optional[str | Path] = None,
        voice_tts: Optional[str | Path] = None,
    ) -> Dict[str, Any]:
        mp = Path(model_path).expanduser().resolve()
        if not mp.exists():
            raise FileNotFoundError(f"model path not found: {mp}")
        out = Path(output_path).expanduser().resolve()
        out.parent.mkdir(parents=True, exist_ok=True)

        probe_candidates = [
            self.repo_root / "benchmarks" / "sigma_gate_lsd" / "results_holdout" / "sigma_gate_lsd.pkl",
            self.repo_root / "benchmarks" / "sigma_gate_lsd" / "results_full" / "sigma_gate_lsd.pkl",
        ]
        probe = next((p for p in probe_candidates if p.is_file()), None)

        stt = Path(voice_stt) if voice_stt else self.repo_root / "models" / "moonshine-base"
        tts = Path(voice_tts) if voice_tts else self.repo_root / "models" / "piper-en"
        voice_added = False

        with tarfile.open(out, "w:gz") as tar:
            cos_dir = self.repo_root / "python" / "cos"
            if cos_dir.is_dir():
                tar.add(cos_dir, arcname="python/cos", recursive=True)
            sigma_dir = self.repo_root / "src" / "sigma"
            if sigma_dir.is_dir():
                tar.add(sigma_dir, arcname="src/sigma", recursive=True)

            tar.add(mp, arcname=f"model/{mp.name}")

            if probe is not None:
                tar.add(probe, arcname="probe/sigma_gate_lsd.pkl")

            config = {
                "mode": "airgap",
                "network": "blocked",
                "telemetry": "disabled",
                "model": mp.name,
                "packaged_from": str(self.repo_root),
            }
            raw = json.dumps(config, indent=2).encode("utf-8")
            info = tarfile.TarInfo(name="config/offline.json")
            info.size = len(raw)
            tar.addfile(info, io.BytesIO(raw))

            if include_voice:
                if stt.is_dir():
                    tar.add(stt, arcname="voice/stt", recursive=True)
                    voice_added = True
                if tts.is_dir():
                    tar.add(tts, arcname="voice/tts", recursive=True)
                    voice_added = True

            launcher = (
                f"#!/usr/bin/env bash\n"
                f"set -euo pipefail\n"
                f'ROOT="$(cd "$(dirname "$0")" && pwd)"\n'
                f"export COS_AIRGAP=1\n"
                f"export COS_OFFLINE=1\n"
                f'export PYTHONPATH="$ROOT/python${{PYTHONPATH:+:$PYTHONPATH}}"\n'
                f"export COS_PROBE=\"${{COS_PROBE:-$ROOT/probe/sigma_gate_lsd.pkl}}\"\n"
                f"# Model file: $ROOT/model/{mp.name}\n"
                f"echo \"[AIRGAP] PYTHONPATH=$PYTHONPATH - run: python3 -m cos offline --verify\"\n"
            ).encode("utf-8")
            sinfo = tarfile.TarInfo(name="start.sh")
            sinfo.size = len(launcher)
            sinfo.mode = 0o755
            tar.addfile(sinfo, io.BytesIO(launcher))

        size_mb = out.stat().st_size / (1024 * 1024)
        return {
            "package": str(out),
            "size_mb": round(float(size_mb), 2),
            "includes_voice": bool(include_voice and voice_added),
            "voice_requested": bool(include_voice),
            "probe_included": probe is not None,
            "airgap": True,
        }

    def deploy(
        self,
        package_path: str | Path,
        target_dir: str | Path = "/opt/creation-os",
    ) -> Dict[str, Any]:
        pkg = Path(package_path).expanduser().resolve()
        if not pkg.is_file():
            raise FileNotFoundError(str(pkg))
        dest = Path(target_dir).expanduser().resolve()
        dest.mkdir(parents=True, exist_ok=True)
        with tarfile.open(pkg, "r:gz") as tar:
            try:
                tar.extractall(dest, filter="data")  # type: ignore[call-arg]
            except TypeError:
                # Python < 3.12
                self._extract_safe_legacy(tar, dest)

        offline = SigmaOffline()
        offline.enforce_airgap()
        return {
            "deployed": True,
            "path": str(dest),
            "start": f"cd {dest} && bash ./start.sh",
            "COS_AIRGAP": os.environ.get("COS_AIRGAP"),
        }

    @staticmethod
    def _extract_safe_legacy(tf: tarfile.TarFile, dest: Path) -> None:
        """Strip ``..`` / absolute paths for older Python."""
        prefix = dest.resolve()
        for m in tf.getmembers():
            p = Path(m.name)
            if m.name.startswith("/") or ".." in p.parts:
                raise ValueError(f"unsafe tar member: {m.name}")
            target = (dest / m.name).resolve()
            if not str(target).startswith(str(prefix)):
                raise ValueError(f"path traversal: {m.name}")
        tf.extractall(dest)


__all__ = ["SigmaOffline", "SigmaOfflinePackager"]
