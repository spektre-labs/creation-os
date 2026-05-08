# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ **fleet**: legacy model registry / routing **plus** enterprise-style **agent** governance.

The **agent** path is a **lab** pattern for per-agent σ history, trust proxies, JSONL audit, and
policy hooks — **not** a certified EU AI Act / NIST attestation and **not** a substitute for
organizational compliance programs (see ``docs/COMPLIANCE.md``). See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

from cos.sigma_gate import ABSTAIN, SigmaGate

__all__ = ["Agent", "BLOCKED", "SigmaFleet"]

BLOCKED = "BLOCKED"


class Agent:
    """One governed agent instance in the fleet."""

    def __init__(self, agent_id: str, model: str, owner: str, *, tier: str = "standard") -> None:
        self.agent_id = str(agent_id)
        self.model = model
        self.owner = str(owner)
        self.tier = str(tier)
        self.sigma_history: List[Dict[str, Any]] = []
        self.total_calls = 0
        self.violations = 0
        self.created = time.time()

    def record(self, sigma: float, verdict: str) -> None:
        sig = float(sigma)
        self.sigma_history.append(
            {"sigma": sig, "σ": sig, "verdict": verdict, "t": time.time()}
        )
        self.total_calls += 1
        if verdict == ABSTAIN:
            self.violations += 1

    def avg_sigma(self) -> float:
        if not self.sigma_history:
            return 0.5
        tail = self.sigma_history[-100:]
        return sum(float(h["sigma"]) for h in tail) / len(tail)

    def avg_σ(self) -> float:  # noqa: PLC2401
        return self.avg_sigma()

    def trust_score(self) -> float:
        if self.total_calls == 0:
            return 0.5
        return round(1.0 - self.violations / self.total_calls, 4)


class SigmaFleet:
    """Named models (routing lab) + **agents** (governance / audit lab)."""

    def __init__(self, gate: Any = None, audit_dir: Optional[str] = None) -> None:
        self.gate = gate or SigmaGate()
        self.models: Dict[str, Dict[str, Any]] = {}
        self._cost_log: List[Dict[str, Any]] = []
        self.agents: Dict[str, Agent] = {}
        self.policies: Dict[str, Dict[str, Any]] = {}
        self.audit_dir = Path(audit_dir or "~/.cos/fleet").expanduser()
        self.audit_dir.mkdir(parents=True, exist_ok=True)
        self.audit_log: List[Dict[str, Any]] = []

    def register(
        self,
        name_or_id: str,
        model: Any = None,
        owner: Optional[str] = None,
        *,
        tier: str = "standard",
        avg_sigma: float = 0.35,
        cost: float = 1.0,
        latency_ms: float = 100.0,
    ) -> Any:
        """Register a **model** for routing (legacy) or an **agent** when ``owner`` is set."""
        if owner is not None:
            agent = Agent(
                str(name_or_id),
                str(model) if model is not None else "",
                owner,
                tier=tier,
            )
            self.agents[str(name_or_id)] = agent
            self._audit("register", str(name_or_id), {"model": model, "tier": tier})
            return agent
        self.models[str(name_or_id)] = {
            "model": model,
            "avg_sigma": float(avg_sigma),
            "cost": float(cost),
            "latency_ms": float(latency_ms),
        }
        return None

    def set_policy(self, policy_name: str, rules: Dict[str, Any]) -> None:
        self.policies[str(policy_name)] = dict(rules)
        self._audit("policy", str(policy_name), dict(rules))

    def score_agent(self, agent_id: str, prompt: str, response: str) -> Dict[str, Any]:
        agent = self.agents.get(str(agent_id))
        if not agent:
            return {"error": "agent not found"}

        sigma, verdict = self.gate.score(str(prompt), str(response))
        agent.record(float(sigma), str(verdict))
        policy_verdict = self._apply_policies(agent, float(sigma), str(verdict))

        result = {
            "agent_id": str(agent_id),
            "σ": round(float(sigma), 4),
            "sigma": round(float(sigma), 4),
            "verdict": policy_verdict,
            "trust_score": agent.trust_score(),
            "total_calls": agent.total_calls,
        }
        self._audit("score", str(agent_id), result)
        return result

    def fleet_status(self) -> Dict[str, Any]:
        agents: List[Dict[str, Any]] = []
        for aid, agent in self.agents.items():
            agents.append(
                {
                    "agent_id": aid,
                    "model": agent.model,
                    "owner": agent.owner,
                    "tier": agent.tier,
                    "avg_σ": round(agent.avg_sigma(), 4),
                    "avg_sigma": round(agent.avg_sigma(), 4),
                    "trust": agent.trust_score(),
                    "calls": agent.total_calls,
                    "violations": agent.violations,
                }
            )
        agents.sort(key=lambda x: x["avg_σ"])
        n = max(len(agents), 1)
        return {
            "total_agents": len(agents),
            "agents": agents,
            "fleet_avg_σ": round(sum(a["avg_σ"] for a in agents) / n, 4),
            "fleet_avg_sigma": round(sum(a["avg_sigma"] for a in agents) / n, 4),
            "fleet_trust": round(sum(a["trust"] for a in agents) / n, 4),
            "policies": list(self.policies.keys()),
        }

    def quarantine(self, agent_id: str, reason: str = "") -> Dict[str, Any]:
        agent = self.agents.get(str(agent_id))
        if agent:
            agent.tier = "quarantined"
            self._audit("quarantine", str(agent_id), {"reason": reason})
            return {"quarantined": True, "agent_id": str(agent_id)}
        return {"error": "agent not found"}

    def promote(self, agent_id: str) -> Dict[str, Any]:
        agent = self.agents.get(str(agent_id))
        if not agent:
            return {"error": "agent not found"}
        if agent.avg_sigma() < 0.2 and agent.trust_score() > 0.9:
            agent.tier = "verified"
            self._audit("promote", str(agent_id), {"new_tier": "verified"})
            return {"promoted": True, "tier": "verified"}
        return {"promoted": False, "reason": "σ too high or trust too low"}

    def sovereign_deploy(self, agent_id: str, jurisdiction: str = "EU") -> Dict[str, Any]:
        agent = self.agents.get(str(agent_id))
        if not agent:
            return {"error": "agent not found"}
        agent.tier = "sovereign"
        self._audit("sovereign", str(agent_id), {"jurisdiction": jurisdiction})
        return {"sovereign": True, "jurisdiction": jurisdiction}

    def compliance_report(self) -> Dict[str, Any]:
        """Aggregate fleet snapshot for **lab** compliance framing (see ``docs/COMPLIANCE.md``)."""
        n = max(len(self.agents), 1)
        avg_σ = sum(a.avg_sigma() for a in self.agents.values()) / n
        return {
            "timestamp": time.time(),
            "fleet_size": len(self.agents),
            "policies_active": len(self.policies),
            "avg_σ": round(avg_σ, 4),
            "avg_sigma": round(avg_σ, 4),
            "quarantined": sum(1 for a in self.agents.values() if a.tier == "quarantined"),
            "sovereign": sum(1 for a in self.agents.values() if a.tier == "sovereign"),
            "total_violations": sum(a.violations for a in self.agents.values()),
            "audit_entries": len(self.audit_log),
            "compliant": (all(a.trust_score() > 0.8 for a in self.agents.values()) if self.agents else True),
        }

    def _apply_policies(self, agent: Agent, sigma: float, verdict: str) -> str:
        if agent.tier == "quarantined":
            return BLOCKED
        for _name, rules in self.policies.items():
            max_s = float(rules.get("max_σ", rules.get("max_sigma", 1.0)))
            if sigma > max_s:
                return BLOCKED
            req = rules.get("require_human_above")
            if req is not None and sigma >= float(req):
                return BLOCKED
        return verdict

    def _audit(self, action: str, target: str, data: Any) -> None:
        entry: Dict[str, Any] = {
            "action": action,
            "target": target,
            "data": data,
            "timestamp": time.time(),
        }
        self.audit_log.append(entry)
        audit_file = self.audit_dir / "audit.jsonl"
        with audit_file.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps(entry, ensure_ascii=False, default=str) + "\n")

    # --- legacy model routing (unchanged contract) ---

    def route(self, query: str, gate: Any) -> Dict[str, Any]:
        if not self.models:
            return {"name": None, "sigma": 1.0, "model": None}
        best: Optional[tuple[float, str, float]] = None
        for name, meta in self.models.items():
            s = float(gate.compute_sigma(None, None, str(query), f"model:{name}"))
            score = s * 0.6 + float(meta["cost"]) * 0.2
            if best is None or score < best[0]:
                best = (score, name, s)
        if best is None:
            return {"name": None, "sigma": 1.0, "model": None}
        _sc, name, sig = best
        return {
            "name": name,
            "sigma": round(sig, 6),
            "model": self.models[name]["model"],
        }

    def cascade_route(self, query: str, gate: Any) -> Dict[str, Any]:
        if not self.models:
            return {"name": None, "sigma": 1.0, "model": None, "cascade_stop": False}
        names = sorted(self.models.keys(), key=lambda n: self.models[n]["cost"])
        ta = float(gate.threshold_accept)
        for n in names:
            s = float(gate.compute_sigma(None, None, str(query), f"cascade:{n}"))
            if s < ta:
                return {
                    "name": n,
                    "sigma": round(s, 6),
                    "model": self.models[n]["model"],
                    "cascade_stop": True,
                }
        n = names[-1] if names else None
        if n is None:
            return {"name": None, "sigma": 1.0, "model": None, "cascade_stop": False}
        s = float(gate.compute_sigma(None, None, str(query), f"cascade:{n}"))
        return {
            "name": n,
            "sigma": round(s, 6),
            "model": self.models[n]["model"],
            "cascade_stop": False,
        }

    def cost_tracking(self, name: str, units: float) -> None:
        self._cost_log.append({"model": str(name), "units": float(units)})

    def sigma_portfolio(self) -> Dict[str, float]:
        return {n: float(v["avg_sigma"]) for n, v in self.models.items()}

    def auto_fallback(self, query: str, gate: Any, order: List[str]) -> Dict[str, Any]:
        for n in order:
            if n not in self.models:
                continue
            s = float(gate.compute_sigma(None, None, str(query), f"fallback:{n}"))
            if s < 0.85:
                return {
                    "name": n,
                    "sigma": round(s, 6),
                    "model": self.models[n]["model"],
                }
        return {"name": None, "sigma": 1.0, "model": None}
