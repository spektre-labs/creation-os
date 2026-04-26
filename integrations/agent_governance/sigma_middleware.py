"""
σ-gate as an optional policy hook next to Microsoft-style agent governance.

Creation OS answers *whether* model output is reliable (semantic σ + action).
A separate agent runtime answers *who* may invoke tools. This module calls
`cos serve` HTTP `POST /v1/verify` so a policy layer can block actions when the
gate says ABSTAIN.

`agent_os` (Microsoft Agent Governance Toolkit patterns) is optional: if the
package is not installed, only `SigmaPolicy` is defined here.
"""

from __future__ import annotations

import json
import os
import urllib.error
import urllib.request
from typing import Any, Dict, Optional

try:
    from agent_os import ExecutionContext, Policy, StatelessKernel  # type: ignore
except ImportError:  # pragma: no cover — optional third-party stack
    StatelessKernel = object  # type: ignore[misc, assignment]
    ExecutionContext = object  # type: ignore[misc, assignment]
    Policy = object  # type: ignore[misc, assignment]


class SigmaPolicy:
    """σ-gate: block agent actions when verification returns ABSTAIN."""

    def __init__(
        self,
        cos_host: str = "127.0.0.1",
        cos_port: int = 3001,
        tau: float = 0.6,
        timeout_s: float = 30.0,
    ) -> None:
        self.cos_url = f"http://{cos_host}:{cos_port}/v1/verify"
        self.tau = float(tau)
        self.timeout_s = float(timeout_s)

    def _verify_payload(self, text: str, model: Optional[str]) -> Dict[str, Any]:
        body: Dict[str, Any] = {"text": text}
        if model:
            body["model"] = model
        data = json.dumps(body).encode("utf-8")
        req = urllib.request.Request(
            self.cos_url,
            data=data,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=self.timeout_s) as resp:
            raw = resp.read().decode("utf-8", errors="replace")
        return json.loads(raw)

    @staticmethod
    def _aggregate_sentences(payload: Dict[str, Any]) -> tuple[float, str]:
        """Return (max σ, strictest action) from cos serve verify JSON."""
        sents = payload.get("sentences") or []
        if not sents:
            return 1.0, "UNKNOWN"
        rank = {"UNKNOWN": 0, "ACCEPT": 1, "RETHINK": 2, "ABSTAIN": 3}
        max_sigma = 0.0
        worst = "ACCEPT"
        for it in sents:
            if not isinstance(it, dict):
                continue
            sig = float(it.get("sigma", 0.0))
            act = str(it.get("action", "UNKNOWN"))
            max_sigma = max(max_sigma, sig)
            if rank.get(act, 0) >= rank.get(worst, 0):
                worst = act
        return max_sigma, worst

    def evaluate_sync(
        self,
        action: str,
        params: Dict[str, Any],
        context: Any = None,
    ) -> Dict[str, Any]:
        """
        Synchronous policy decision.

        Parameters mirror async `evaluate` for Agent OS–style call sites.
        """
        del action, context
        text = str(
            params.get("text", params.get("prompt", params.get("message", "")))
            or ""
        ).strip()
        if not text:
            return {"allow": True, "reason": "no text to verify"}

        model = params.get("model")
        if model is not None:
            model = str(model)

        try:
            result = self._verify_payload(text, model)
        except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError, OSError) as e:
            return {"allow": False, "reason": f"verify_unreachable:{e!s}"}
        except json.JSONDecodeError:
            return {"allow": False, "reason": "verify_bad_json"}

        sigma, action_verdict = self._aggregate_sentences(result)
        if action_verdict == "ABSTAIN" or sigma > self.tau:
            return {
                "allow": False,
                "reason": f"σ-gate {action_verdict} (σ={sigma:.3f})",
                "sigma": sigma,
                "action": action_verdict,
            }
        return {"allow": True, "sigma": sigma, "action": action_verdict}

    async def evaluate(
        self,
        action: str,
        params: Dict[str, Any],
        context: Any = None,
    ) -> Dict[str, Any]:
        """Async wrapper (Agent OS style); runs sync verify in-process."""
        return self.evaluate_sync(action, params, context)


def example_usage_snippet() -> None:
    """Documented pattern; requires optional `agent_os` package."""
    _ = os.environ.get("CREATION_OS_ROOT", ".")
    # kernel = StatelessKernel()
    # ctx = ExecutionContext(
    #     agent_id="analyst-1",
    #     policies=[Policy.read_only(), SigmaPolicy(tau=0.5)],
    # )
