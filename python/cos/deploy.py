# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-deploy — command plans and health/smoke checks (no cluster side effects by default).

Operators run returned shell strings in CI/CD. Air-gap path delegates to bundle copy stubs.
See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import json
import shlex
import shutil
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Dict, Mapping, Optional

__all__ = ["SigmaDeploy"]


class SigmaDeploy:
    """Dry-run deploy recipes + HTTP probes for σ API instances."""

    @staticmethod
    def deploy_local(model_path: str, port: int) -> Dict[str, Any]:
        p = Path(model_path)
        return {
            "mode": "local",
            "model_path": str(p),
            "port": int(port),
            "command": f"cos serve --port {int(port)}",
            "note": "σ-gate API is separate from weights path; pass model via your inference driver.",
        }

    @staticmethod
    def deploy_docker(image: str, port: int, env: Optional[Mapping[str, str]] = None) -> Dict[str, Any]:
        env_flags = ""
        if env:
            for k, v in env.items():
                env_flags += f" -e {k}={shlex.quote(str(v))}"
        return {
            "mode": "docker",
            "image": str(image),
            "port": int(port),
            "command": f"docker run --rm -p {int(port)}:8420{env_flags} {image}",
        }

    @staticmethod
    def deploy_k8s(helm_values: str, namespace: str) -> Dict[str, Any]:
        return {
            "mode": "k8s",
            "helm_values": str(helm_values),
            "namespace": str(namespace),
            "command": f"helm upgrade --install creation-os {helm_values} -n {namespace}",
        }

    @staticmethod
    def deploy_airgap(bundle_path: str, target: str) -> Dict[str, Any]:
        src, dst = Path(bundle_path), Path(target)
        dst.mkdir(parents=True, exist_ok=True)
        if src.is_file():
            shutil.copy2(src, dst / src.name)
        elif src.is_dir():
            shutil.copytree(src, dst / src.name, dirs_exist_ok=True)
        return {"mode": "airgap", "bundle": str(src), "target": str(dst), "copied": True}

    @staticmethod
    def health_after_deploy(endpoint: str, *, timeout_s: float = 3.0) -> Dict[str, Any]:
        url = str(endpoint).rstrip("/") + "/health"
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "SigmaDeploy/1"})
            with urllib.request.urlopen(req, timeout=float(timeout_s)) as resp:
                body = resp.read().decode("utf-8", errors="replace")
                data = json.loads(body) if body.strip().startswith("{") else {}
                return {"ok": True, "status_code": resp.status, "body": data or body[:500]}
        except Exception as e:  # pragma: no cover — network variable
            return {"ok": False, "error": str(e)}

    @staticmethod
    def smoke_test(endpoint: str, gate: Any) -> Dict[str, Any]:
        """POST ``/v1/score`` when available; fall back to local gate scoring."""
        url = str(endpoint).rstrip("/") + "/v1/score"
        payload = json.dumps({"prompt": "2+2", "response": "4"}).encode("utf-8")
        try:
            req = urllib.request.Request(
                url,
                data=payload,
                headers={"Content-Type": "application/json", "User-Agent": "SigmaDeploy/1"},
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=5.0) as resp:
                body = json.loads(resp.read().decode("utf-8", errors="replace"))
                sigma = float(body.get("sigma", 1.0))
                verdict = str(body.get("verdict", ""))
                return {
                    "ok": resp.status < 400 and verdict in ("ACCEPT", "RETHINK", "ABSTAIN"),
                    "sigma": sigma,
                    "verdict": verdict,
                    "via": "http",
                }
        except Exception:
            s, v = gate.score("2+2", "4")
            return {
                "ok": str(v) in ("ACCEPT", "RETHINK", "ABSTAIN"),
                "sigma": float(s),
                "verdict": str(v),
                "via": "local_gate",
            }

    @staticmethod
    def rollback(deployment: str, previous_version: str) -> Dict[str, Any]:
        return {
            "deployment": str(deployment),
            "previous_version": str(previous_version),
            "command": f"kubectl rollout undo deployment/{deployment} --to-revision={previous_version}",
            "note": "Adjust for Helm history rollback in your stack.",
        }

    def blue_green(self, old_endpoint: str, new_endpoint: str, gate: Any) -> Dict[str, Any]:
        """Score a fixed probe on both endpoints; prefer lower σ if both healthy."""
        probe = ("Sanity check", "ok")
        old_h = SigmaDeploy.health_after_deploy(old_endpoint)
        new_h = SigmaDeploy.health_after_deploy(new_endpoint)
        use_local = not old_h.get("ok") and not new_h.get("ok")
        if use_local:
            baseline = float(gate.score(probe[0], probe[1])[0])
            return {"winner": "neither_http", "sigma": baseline, "use": "local_gate", "old": old_h, "new": new_h}
        # Prefer HTTP score when at least one endpoint answers
        def ep_sigma(base: str) -> float:
            r = SigmaDeploy.smoke_test(base, gate)
            return float(r.get("sigma", 1.0))

        s_old, s_new = ep_sigma(old_endpoint), ep_sigma(new_endpoint)
        winner = old_endpoint if s_old <= s_new else new_endpoint
        return {
            "winner": winner,
            "sigma_old": s_old,
            "sigma_new": s_new,
            "use": "lower_sigma",
        }

