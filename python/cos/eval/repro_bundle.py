# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""SHA-pinned, timestamped reproducibility bundle for measured claims.

Aligns with ``docs/CLAIM_DISCIPLINE.md`` and ``docs/REPRO_BUNDLE_TEMPLATE.md``:
registered claims, results, **mandatory negatives**, limitations, and falsifiers."""
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from cos.sigma_gate import SigmaGate

__all__ = ["ReproBundle"]


class ReproBundle:
    """One self-contained evidence package: metadata + claims + results + negatives."""

    def __init__(
        self,
        name: str,
        gate: Optional[Any] = None,
        output_dir: Optional[Union[str, Path]] = None,
    ) -> None:
        self.name = str(name)
        self.gate = gate if gate is not None else SigmaGate()
        self.output_dir = Path(output_dir or f"eval_results/{self.name}")
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.bundle: Dict[str, Any] = {
            "name": self.name,
            "timestamp": time.time(),
            "git_sha": self._git_sha(),
            "gate_version": "1.0.0",
            "python_version": self._python_version(),
            "claims": [],
            "results": [],
            "negatives": [],
            "limitations": [],
            "falsifiers": [],
        }

    def claim(self, statement: str, evidence_level: Union[int, str]) -> None:
        """Register a claim with evidence ladder level (1–7) or string label."""
        self.bundle["claims"].append(
            {
                "statement": str(statement),
                "evidence_level": evidence_level,
                "timestamp": time.time(),
            }
        )

    def result(
        self,
        benchmark: str,
        metric: str,
        value: Optional[float],
        n_samples: int,
        *,
        model_id: str = "",
        config: Optional[Dict[str, Any]] = None,
    ) -> None:
        """Record a measured result row."""
        entry: Dict[str, Any] = {
            "benchmark": str(benchmark),
            "metric": str(metric),
            "value": round(float(value), 4) if value is not None else None,
            "n_samples": int(n_samples),
            "model_id": str(model_id),
            "config": dict(config or {}),
            "git_sha": self._git_sha(),
            "timestamp": time.time(),
        }
        self.bundle["results"].append(entry)

    def negative(self, benchmark: str, metric: str, value: Optional[float], explanation: str) -> None:
        """Record a **negative** result (required for claim discipline)."""
        self.bundle["negatives"].append(
            {
                "benchmark": str(benchmark),
                "metric": str(metric),
                "value": round(float(value), 4) if value is not None else None,
                "explanation": str(explanation),
                "timestamp": time.time(),
            }
        )

    def limitation(self, description: str) -> None:
        self.bundle["limitations"].append({"description": str(description), "timestamp": time.time()})

    def falsifier(self, description: str) -> None:
        """Record what observation would refute the bundled claims."""
        self.bundle["falsifiers"].append({"description": str(description), "timestamp": time.time()})

    def validate(self) -> Dict[str, Any]:
        """Return whether the bundle satisfies minimum claim-discipline checks."""
        errors: List[str] = []
        if not self.bundle["claims"]:
            errors.append("No claims registered")
        if not self.bundle["results"]:
            errors.append("No results recorded")
        if not self.bundle["negatives"]:
            errors.append("NO NEGATIVES — claim discipline violation")
        if not self.bundle["falsifiers"]:
            errors.append("No falsifiers — not falsifiable")
        if not self.bundle["git_sha"]:
            errors.append("No git SHA — not reproducible")
        return {
            "valid": len(errors) == 0,
            "errors": errors,
            "claims": len(self.bundle["claims"]),
            "results": len(self.bundle["results"]),
            "negatives": len(self.bundle["negatives"]),
        }

    def save(self) -> str:
        """Write JSON to disk; ``bundle_sha`` is the SHA-256 (truncated) of the saved payload."""
        body = {k: v for k, v in self.bundle.items() if k != "bundle_sha"}
        content = json.dumps(body, indent=2, sort_keys=True)
        digest = hashlib.sha256(content.encode()).hexdigest()[:16]
        self.bundle["bundle_sha"] = digest
        path = self.output_dir / f"bundle_{self.name}_{digest}.json"
        path.write_text(json.dumps(self.bundle, indent=2, sort_keys=True), encoding="utf-8")
        return str(path)

    @staticmethod
    def _git_sha() -> str:
        try:
            r = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                capture_output=True,
                text=True,
                timeout=5,
                check=False,
            )
            return r.stdout.strip() if r.returncode == 0 else ""
        except (OSError, subprocess.SubprocessError):
            return ""

    @staticmethod
    def _python_version() -> str:
        return f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}"
