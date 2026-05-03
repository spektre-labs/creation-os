# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-offline — air-gap bundle manifests (SHA-256) and lab deploy helpers.

Does not embed a full model packager; records paths + hashes for audit. See
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import socket
import time
from pathlib import Path
from typing import Any, Dict

__all__ = ["SigmaOffline"]


class SigmaOffline:
    """Self-contained bundle descriptor + verify/deploy stubs."""

    MODEL_FORMATS: tuple[str, ...] = ("GGUF", "SafeTensors", "ONNX")

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()

    @staticmethod
    def detect_mode_from_env() -> bool:
        """True when host marks CREATION_OS_OFFLINE=1 (policy flag; not a kernel probe)."""
        return (os.environ.get("CREATION_OS_OFFLINE") or "").strip() == "1"

    def package(
        self,
        model: Dict[str, Any],
        gate: Dict[str, Any],
        probes: Dict[str, Any],
        codex: Dict[str, Any],
    ) -> Dict[str, Any]:
        """Return a manifest dict (write with :meth:`write_bundle`)."""
        body = {
            "model": dict(model),
            "gate": dict(gate),
            "probes": dict(probes),
            "codex": dict(codex),
            "created_at": time.time(),
        }
        raw = json.dumps(body, sort_keys=True, default=str).encode("utf-8")
        return {
            "manifest_version": 1,
            "sha256": hashlib.sha256(raw).hexdigest(),
            "payload": body,
            "model_formats_supported": list(self.MODEL_FORMATS),
        }

    def write_bundle(self, bundle: Dict[str, Any], target_dir: Path) -> Path:
        d = Path(target_dir)
        d.mkdir(parents=True, exist_ok=True)
        p = d / "creation_os_bundle.json"
        p.write_text(json.dumps(bundle, indent=2, default=str), encoding="utf-8")
        hp = d / "creation_os_bundle.sha256"
        hp.write_text(str(bundle.get("sha256", "")) + "\n", encoding="utf-8")
        return p

    def verify_bundle(self, bundle_path: Path) -> Dict[str, Any]:
        p = Path(bundle_path)
        data = json.loads(p.read_text(encoding="utf-8"))
        stated = str(data.get("sha256", ""))
        payload = data.get("payload")
        raw = json.dumps(payload, sort_keys=True, default=str).encode("utf-8")
        actual = hashlib.sha256(raw).hexdigest()
        ok = stated == actual
        return {"ok": bool(ok), "stated": stated, "actual": actual, "path": str(p)}

    def deploy_airgap(
        self,
        bundle_path: Path,
        target_dir: Path,
        *,
        verify: bool = True,
    ) -> Dict[str, Any]:
        if verify:
            vr = self.verify_bundle(bundle_path)
            if not vr["ok"]:
                return {"deployed": False, "verify": vr}
        dest = Path(target_dir)
        dest.mkdir(parents=True, exist_ok=True)
        shutil.copy2(bundle_path, dest / bundle_path.name)
        return {"deployed": True, "target": str(dest)}

    def no_network_check(self, *, timeout: float = 0.35) -> Dict[str, Any]:
        """Best-effort **heuristic**; set COS_FORCE_AIRGAP_OK=1 to skip probe in CI."""
        if (os.environ.get("COS_FORCE_AIRGAP_OK") or "").strip() == "1":
            return {"reachable": False, "ok_for_airgap_policy": True, "skipped": True}
        try:
            sock = socket.create_connection(("1.1.1.1", 53), timeout=timeout)
            sock.close()
            return {"reachable": True, "ok_for_airgap_policy": False}
        except OSError:
            return {"reachable": False, "ok_for_airgap_policy": True}

    def bundle_trust_sigma(self, bundle: Dict[str, Any]) -> Dict[str, Any]:
        """σ on canonical manifest string — high σ ⇒ treat as “review bundle” (lab)."""
        blob = json.dumps(bundle, sort_keys=True, default=str)[:8000]
        sigma = float(self.gate.compute_sigma(None, None, "offline_bundle", blob))
        verdict = str(self.gate._verdict(sigma))
        return {"sigma": round(sigma, 6), "verdict": verdict}

    def update_mechanism_note(self) -> str:
        return (
            "USB delivery: new bundle → verify_bundle → deploy_airgap → swap symlink "
            "(operator runbook; not automated here)."
        )
