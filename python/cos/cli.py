# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Creation OS **cos** CLI (minimal). Heavy teacher/student wiring stays in harness scripts;
this entrypoint performs JSONL I/O and optional mock distillation for CI smoke tests.

Run: ``PYTHONPATH=python python -m cos …`` or ``./scripts/cos …`` from the repo root.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, Iterator, List, Mapping, Optional, Tuple

_COS_HELP_EPILOG = """
command groups (surface for first contact; many lab subcommands also exist):
  CORE            score, chat, think, bench, serve, version, identity
  ANALYSIS        explain, cascade, calibrate
  INFRASTRUCTURE  health, hardware, layers, registry, cost
  ADVANCED        graph, evolve, redteam

Exit codes (where implemented): 0 ok, 1 error / usage, 2 σ-gate ABSTAIN (score/gate).
""".strip()


def _cli_out_json(ns: argparse.Namespace) -> bool:
    return bool(
        getattr(ns, "out_json", False)
        or getattr(ns, "score_as_json", False)  # backward compat
    )


def _cli_verbose(ns: argparse.Namespace) -> bool:
    return bool(getattr(ns, "cli_verbose", False))


def _iter_prompts_jsonl(path: Path) -> Iterator[str]:
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(obj, dict):
                p = obj.get("prompt") or obj.get("text") or obj.get("instruction")
                if isinstance(p, str) and p.strip():
                    yield p.strip()


class _LabFirewallEncodeModel:
    """Deterministic ``sigma_hints`` for ``cos firewall`` / CI (no torch)."""

    def encode(self, text: str, *, output_hidden_states: bool = True):
        _ = output_hidden_states
        t = str(text).lower()
        if t == "ok":
            return {"sigma_hints": {"input": 0.12, "tool": 0.11, "memory": 0.05}}
        if "ignore previous" in t or "delete all files" in t:
            return {"sigma_hints": {"input": 0.92, "tool": 0.15, "memory": 0.15}}
        if "ignore safety" in t or "rm -rf" in t:
            return {"sigma_hints": {"input": 0.12, "tool": 0.94, "memory": 0.12}}
        if "memory_trojan" in t:
            return {"sigma_hints": {"input": 0.12, "tool": 0.12, "memory": 0.96}}
        return {"sigma_hints": {"input": 0.12, "tool": 0.11, "memory": 0.13}}

    def generate(self, prompt: str) -> str:
        return ""


def _firewall_layer_from_message(msg: str) -> int:
    if "TOOL_OUTPUT" in msg or "injection suspected" in msg:
        return 2
    if "MEMORY_BLOCKED" in msg:
        return 3
    if "OUTPUT_ABSTAIN" in msg:
        return 4
    return 1


def _cmd_firewall(args: argparse.Namespace) -> int:
    from cos.sigma_firewall import SigmaFirewall
    from cos.sigma_gate_core import SigmaState

    chk = getattr(args, "check", None)
    ctool = getattr(args, "check_tool", None)
    if chk is None and ctool is None:
        print("cos firewall: pass --check TEXT or --check-tool TEXT", file=sys.stderr)
        return 2

    fw = SigmaFirewall()
    st = SigmaState()
    model = _LabFirewallEncodeModel()

    if chk is not None:
        ok, sigma, msg = fw.check_input(str(chk), model, st)
    else:
        ok, sigma, msg = fw.check_tool_output(str(ctool), model, st)

    layer = _firewall_layer_from_message(msg)
    payload = {
        "blocked": not ok,
        "layer": layer,
        "sigma": float(sigma),
        "reason": msg,
    }
    status = "BLOCKED" if not ok else "PASS"
    print(
        f"{status}: σ={float(sigma):.4f}, layer={layer}, {msg.split(':', 1)[0]}",
        file=sys.stderr,
    )
    print(json.dumps(payload, ensure_ascii=False))
    return 4 if not ok else 0


def _cmd_tool_safety(args: argparse.Namespace) -> int:
    from cos.tool_safety import ToolSafety

    tool = str(getattr(args, "ts_tool", "") or "").strip()
    if not tool:
        print("cos tool-safety: --tool NAME is required", file=sys.stderr)
        return 2
    astr = str(getattr(args, "ts_args", "") or "")
    intent = str(getattr(args, "ts_intent", "") or "")
    ts = ToolSafety()
    out = ts.sigma_before_execute(tool, astr, intent)
    print(json.dumps(out, ensure_ascii=False, default=str))
    return 0


def _cmd_identity(args: argparse.Namespace) -> int:
    """Print Engram-backed session count, narrative size, identity σ, and continuity axes."""
    from cos.engram import Engram

    raw = str(getattr(args, "identity_path", "") or "").strip()
    e = Engram(path=raw) if raw else Engram()
    print(f"Sessions: {e.identity['sessions']}")
    print(f"Events: {len(e.narrative)}")
    print(f"Identity σ: {e.identity_σ()}")
    inv = e.identity_invariant()
    print(
        f"Identity invariant (σ): preserved={inv['identity_preserved']} "
        f"sigma_avg={inv.get('sigma_avg', 'N/A')} drift={inv.get('drift', 'N/A')}"
    )
    cont = e.continuity_check()
    print(f"Continuity: {cont['continuity_score']}")
    for axis, val in cont["axes"].items():
        mark = "+" if val else "-"
        print(f"  {mark} {axis}")
    return 0


def _cmd_distill_generate(args: argparse.Namespace) -> int:
    from cos.sigma_distill import SigmaDistill

    prompts_path = Path(args.prompts)
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    if args.mock:

        class _MockTeacher:
            def generate(self, prompt: str) -> str:
                return f"mock_response::{prompt[:256]}"

        class _ConstGate:
            def compute_sigma(self, _teacher: Any, _prompt: str, _output: str) -> float:
                return float(args.mock_sigma)

        teacher: Any = _MockTeacher()
        gate: Any = _ConstGate()
    else:
        print(
            "cos distill generate: non-mock mode requires a wired SigmaDistill in a harness "
            "(install transformers, set CREATION_* env, etc.). Use --mock for JSONL smoke.",
            file=sys.stderr,
        )
        return 2

    student: Any = object()
    sd = SigmaDistill(teacher=teacher, student=student, gate=gate, k_raw=float(args.k_raw))
    rows = sd.generate_training_data(_iter_prompts_jsonl(prompts_path), limit=int(args.limit))
    with out_path.open("w", encoding="utf-8") as out:
        for row in rows:
            out.write(json.dumps(row, ensure_ascii=False) + "\n")
    print(f"wrote {len(rows)} rows to {out_path}")
    return 0


def _cmd_distill_train(args: argparse.Namespace) -> int:
    data_path = Path(args.data)
    epochs = max(1, int(args.epochs))

    class _CounterStudent:
        def __init__(self) -> None:
            self.n = 0
            self.weight_sum = 0.0

        def train_step(self, prompt: str, response: str, *, weight: float = 1.0) -> float:
            self.n += 1
            self.weight_sum += float(weight)
            return 0.0

    from cos.sigma_distill import SigmaDistill

    rows: List[Dict[str, Any]] = []
    with data_path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError:
                continue

    stu = _CounterStudent()
    dummy_teacher: Any = object()
    dummy_gate: Any = object()

    sd = SigmaDistill(teacher=dummy_teacher, student=stu, gate=dummy_gate)
    sd.distill(rows, epochs=epochs)
    print(f"train_step calls: {stu.n} total_weight: {stu.weight_sum:.6f}")
    return 0


def _cmd_distill_eval(args: argparse.Namespace) -> int:
    print(
        "cos distill eval: stub — wire benchmarks/sigma_gate_eval or lm-eval harness "
        f"(student={args.student!r} benchmark={args.benchmark!r}).",
        file=sys.stderr,
    )
    return 0


def _cmd_debate(args: argparse.Namespace) -> int:
    from cos.sigma_debate import SigmaDebate

    if not args.mock:
        print(
            "cos debate: use --mock for JSON stdout smoke, or wire HF / vLLM deputies in a harness.",
            file=sys.stderr,
        )
        return 2

    names = [x.strip() for x in str(args.models).split(",") if x.strip()]
    if len(names) < 2:
        print("cos debate: need at least two comma-separated names in --models", file=sys.stderr)
        return 2

    class _ArgDeputy:
        def __init__(self, tag: str) -> None:
            self.tag = tag

        def argue(self, question: str, history: List[Dict[str, Any]]) -> str:
            opp = len(history)
            return f"{self.tag}:{question[:80]}:r{opp}"

    class _SkewGate:
        """Lower σ for the first deputy so mock debates pick a winner."""

        def compute_sigma(self, model: Any, _q: str, _arg: str) -> float:
            if isinstance(model, _ArgDeputy) and model.tag == names[0]:
                return 0.22
            return 0.48

    a = _ArgDeputy(names[0])
    b = _ArgDeputy(names[1])
    sd = SigmaDebate(a, b, _SkewGate())
    text, side, stat = sd.debate(str(args.question), rounds=int(args.rounds))
    print(json.dumps({"winner_side": side, "stat": stat, "text": text}, ensure_ascii=False))
    return 0


def _cmd_self_play(args: argparse.Namespace) -> int:
    from cos.sigma_selfplay import SigmaSelfPlay

    if not args.mock:
        print(
            "cos self-play: use --mock for JSONL smoke, or wire a full model with generate/critique.",
            file=sys.stderr,
        )
        return 2

    class _SelfModel:
        def generate(self, question: str, temperature: float = 0.5) -> str:
            return f"t{temperature:.1f}:{question[:48]}"

        def critique(self, question: str, answer: str) -> str:
            return f"critique_of({answer[:24]})"

    class _Gate:
        def compute_sigma(self, _m: Any, _q: str, text: str) -> float:
            if text.startswith("t0.3"):
                return 0.25
            if text.startswith("critique"):
                return 0.55
            return 0.45

    sp = SigmaSelfPlay(_Gate())
    out_path = Path(args.output) if getattr(args, "output", None) else None
    rows: List[Dict[str, Any]] = []
    prompts_path = Path(args.prompts) if args.prompts else None
    if prompts_path is not None and prompts_path.is_file():
        for pq in _iter_prompts_jsonl(prompts_path):
            ans, sig = sp.play(_SelfModel(), pq)
            rows.append({"prompt": pq, "answer": ans, "sigma": float(sig)})
    else:
        pq = str(args.question or "").strip() or "default prompt"
        ans, sig = sp.play(_SelfModel(), pq)
        rows.append({"prompt": pq, "answer": ans, "sigma": float(sig)})

    if out_path is not None:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with out_path.open("w", encoding="utf-8") as f:
            for row in rows:
                f.write(json.dumps(row, ensure_ascii=False) + "\n")
        print(f"wrote {len(rows)} rows to {out_path}")
    else:
        print(json.dumps(rows, ensure_ascii=False))
    return 0


def _cmd_proconductor(args: argparse.Namespace) -> int:
    from cos.sigma_node_manager import NodeManager, default_stack_path, load_node_stack_config
    from cos.sigma_proconductor import ProconductorDebate

    _ = args.all_models
    stack_path = str(getattr(args, "node_stack", "") or "").strip()
    cfg_path = Path(stack_path).expanduser() if stack_path else default_stack_path()

    if getattr(args, "stack", False):
        data = load_node_stack_config(cfg_path)
        orch = data.get("orchestration") or {}
        out = {
            "config_path": str(cfg_path),
            "nodes": sorted(list((data.get("nodes") or {}).keys())),
            "proconductor": orch.get("proconductor", ""),
            "sigma_consensus": orch.get("sigma_consensus", {}),
            "default_order": orch.get("default_order", []),
        }
        print(json.dumps(out, ensure_ascii=False, indent=2))
        return 0

    if getattr(args, "firmware_scan", False):
        nid = str(getattr(args, "node", "") or "").strip()
        if not nid:
            print("cos proconductor --firmware-scan requires --node ID", file=sys.stderr)
            return 2
        data = load_node_stack_config(cfg_path)
        nm = NodeManager(data)
        if nid not in nm.nodes:
            print(f"cos proconductor: unknown node {nid!r}", file=sys.stderr)
            return 2
        sample = str(getattr(args, "sample_text", "") or "")
        warns = nm.check_firmware(sample, dict(nm.nodes[nid]))
        print(json.dumps({"node": nid, "warnings": warns}, ensure_ascii=False, indent=2))
        return 0

    task = str(getattr(args, "task", "") or "").strip()
    if task:
        data = load_node_stack_config(cfg_path)
        nm = NodeManager(data)
        if getattr(args, "triangulate", False):
            print(json.dumps(nm.triangulate(task), ensure_ascii=False, indent=2, default=str))
            return 0
        if getattr(args, "auto_route", False):
            node_id = nm.route(task, {})
            row = nm.execute_with_sigma(node_id, task)
            print(json.dumps({"routed": node_id, "execution": row}, ensure_ascii=False, indent=2, default=str))
            return 0
        print("cos proconductor: with --task add --triangulate and/or --auto-route", file=sys.stderr)
        return 2

    if not args.mock:
        print(
            "cos proconductor: use --mock --question … for legacy four-deputy JSON, "
            "or --stack / --task … / --firmware-scan … for Spektre node stack lab.",
            file=sys.stderr,
        )
        return 2
    if not str(args.question or "").strip():
        print("cos proconductor: legacy mock mode requires non-empty --question", file=sys.stderr)
        return 2

    class _Nm:
        def __init__(self, name: str, answer: str) -> None:
            self.name = name
            self._answer = answer

        def generate(self, question: str) -> str:
            _ = question
            return self._answer

    class _PGate:
        def compute_sigma(self, model: Any, _q: str, answer: str) -> float:
            if "consensus" in answer:
                return 0.2
            return 0.75

    models = [
        _Nm("bitnet", "consensus path alpha"),
        _Nm("qwen3", "consensus path alpha"),
        _Nm("gemma3", "consensus path alpha"),
        _Nm("deepseek", "lonely road"),
    ]
    pc = ProconductorDebate(models, _PGate())
    ans, sig = pc.multi_debate(str(args.question))
    print(json.dumps({"answer": ans, "mean_sigma": sig}, ensure_ascii=False))
    return 0


def _cmd_report(args: argparse.Namespace) -> int:
    from cos.sigma_audit import SigmaAudit

    aud = SigmaAudit(path=str(args.audit_dir))
    if getattr(args, "firewall", False):
        cap = max(4000, int(args.tail) * 50)
        print(json.dumps(aud.firewall_stats(max_lines=cap), ensure_ascii=False))
        return 0
    if not getattr(args, "agent", False):
        print(
            "cos report: pass --agent for recent σ-agent audit JSONL lines, "
            "or --firewall for σ-firewall audit statistics.",
            file=sys.stderr,
        )
        return 2
    for line in aud.iter_recent_lines(max_lines=int(args.tail)):
        print(line)
    return 0


def _cmd_swarm(args: argparse.Namespace) -> int:
    from cos.sigma_quorum import SigmaQuorum
    from cos.sigma_stigmergy import SigmaStigmergy
    from cos.sigma_swarm_agent import SigmaSwarmAgent, SwarmTask

    envp = Path(str(getattr(args, "env_file", "") or "~/.cos/swarm_stigmergy.json")).expanduser()
    persist = not bool(getattr(args, "no_persist", False))
    st_path: Optional[Path] = envp if persist else None

    if getattr(args, "env", False):
        if not persist:
            print(
                "cos swarm --env requires persisted state (omit --no-persist for this session).",
                file=sys.stderr,
            )
            return 2
        stig = SigmaStigmergy(persist_path=envp)
        if getattr(args, "env_show", False):
            snap = stig.strongest_signals(prefix=None, n=1000)
            print(
                json.dumps(
                    {"path": str(envp), "count": len(stig.environment), "signals": snap},
                    ensure_ascii=False,
                    indent=2,
                    default=str,
                )
            )
            return 0
        dr = getattr(args, "decay_rate", None)
        if dr is not None:
            removed = stig.decay(float(dr))
            print(
                json.dumps(
                    {
                        "path": str(envp),
                        "decay_rate": float(dr),
                        "removed": int(removed),
                        "remaining": len(stig.environment),
                    },
                    ensure_ascii=False,
                    indent=2,
                )
            )
            return 0
        print("cos swarm --env needs --show or --decay RATE", file=sys.stderr)
        return 2

    n_agents = int(getattr(args, "agents", 0) or 0)
    task_g = str(getattr(args, "task", "") or "").strip()
    if n_agents > 0 and task_g:
        stig = SigmaStigmergy(persist_path=st_path)
        domain = str(getattr(args, "domain", "") or "lab")
        tid = str(getattr(args, "task_id", "") or "t1")

        class _SwModel:
            def generate(self, goal: str, *, context=None):
                _ = context
                return "summary:" + str(len(str(goal).strip()))

        class _SwGate:
            def compute_sigma(self, model: Any, goal: str, output: str) -> float:
                _ = model, output
                if "BADX" in str(goal).upper():
                    return 0.99
                return 0.18

        agents = [SigmaSwarmAgent(f"a{i}", _SwModel(), _SwGate(), stig) for i in range(n_agents)]
        q = SigmaQuorum()
        task = SwarmTask(goal=task_g, domain=domain, id=tid)
        out = q.reach_consensus(agents, task, min_agreement=int(getattr(args, "quorum", 3) or 3))
        print(json.dumps(out, ensure_ascii=False, indent=2, default=str))
        return 0 if out.get("consensus") else 3

    print(
        "cos swarm: use --agents N --task STR [--quorum K] [--domain … --task-id …] "
        "or --env --show / --env --decay RATE [--env-file PATH]",
        file=sys.stderr,
    )
    return 2


def _default_sovereign_state_path() -> Path:
    return Path("~/.cos/sovereign_lab.json").expanduser()


def _load_sovereign_session(path: Path) -> Dict[str, Any]:
    if not path.is_file():
        return {"sovereign": {}, "circuit": {}, "config": {}, "last_goal": ""}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {"sovereign": {}, "circuit": {}, "config": {}, "last_goal": ""}
    if not isinstance(data, dict):
        return {"sovereign": {}, "circuit": {}, "config": {}, "last_goal": ""}
    data.setdefault("sovereign", {})
    data.setdefault("circuit", {})
    data.setdefault("config", {})
    data.setdefault("last_goal", "")
    return data


def _save_sovereign_session(path: Path, data: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False, default=str), encoding="utf-8")


def _cmd_sovereign(args: argparse.Namespace) -> int:
    from cos.sigma_agent import AgentAction
    from cos.sigma_circuit_breaker import SigmaCircuitBreaker
    from cos.sigma_containment import SigmaContainment
    from cos.sigma_gate_core import Q16, SigmaState, sigma_q16
    from cos.sigma_sovereign import SigmaSovereign

    path = Path(str(getattr(args, "state_file", "") or "~/.cos/sovereign_lab.json")).expanduser()
    raw = _load_sovereign_session(path)
    base_cfg = dict(raw.get("config") or {})
    base_cfg["max_actions"] = int(getattr(args, "max_actions", 20) or 20)
    sv = SigmaSovereign.from_snapshot(None, base_cfg, raw.get("sovereign") or {})
    cb = SigmaCircuitBreaker.from_snapshot(raw.get("circuit") or {})
    ct = SigmaContainment()

    if getattr(args, "halt", False):
        sv.halt("operator halt")
        raw["sovereign"] = sv.snapshot()
        raw["circuit"] = cb.snapshot()
        raw["config"] = base_cfg
        _save_sovereign_session(path, raw)
        print(json.dumps({"halted": True, "path": str(path)}, ensure_ascii=False))
        return 0

    if getattr(args, "status", False) or getattr(args, "circuit_status", False):
        out = {
            "path": str(path),
            "sovereign": sv.snapshot(),
            "circuit": cb.snapshot(),
            "last_goal": raw.get("last_goal", ""),
        }
        print(json.dumps(out, ensure_ascii=False, indent=2))
        return 0

    goal = str(getattr(args, "goal", "") or "").strip()
    n_sim = int(getattr(args, "simulate_steps", 0) or 0)

    if goal and n_sim > 0:
        act = AgentAction("execute", {"cmd": goal})
        trail: List[Dict[str, Any]] = []
        for i in range(n_sim):
            st = SigmaState()
            probe = min(0.95, 0.12 + float(i) * 0.04)
            st.sigma = sigma_q16(probe)
            st.d_sigma = 1 if i >= n_sim - 3 else 0
            st.k_eff = Q16
            pol, msg = sv.check_autonomy(act, st)
            cb_out, cb_msg = cb.check(probe, None)
            trail.append(
                {
                    "step": i,
                    "sigma_probe": float(probe),
                    "policy": pol,
                    "policy_msg": msg,
                    "autonomy_level": float(sv.autonomy_level),
                    "circuit": cb_out,
                    "circuit_msg": cb_msg,
                }
            )
            raw["sovereign"] = sv.snapshot()
            raw["circuit"] = cb.snapshot()
            raw["config"] = base_cfg
            raw["last_goal"] = goal
            _save_sovereign_session(path, raw)
            if pol == "HALT":
                break
        destructive = AgentAction("delete", {"path": "/tmp/x"})
        applied = sv.apply_autonomy(destructive)
        contained = ct.contain(str(destructive), float(sv.snapshot()["avg_sigma"]), None)
        print(
            json.dumps(
                {
                    "trail": trail,
                    "apply_delete": applied,
                    "containment": contained,
                    "sovereign": sv.snapshot(),
                    "circuit": cb.snapshot(),
                },
                ensure_ascii=False,
                indent=2,
                default=str,
            )
        )
        return 0 if not sv.halted else 4

    if goal:
        st = SigmaState()
        st.sigma = sigma_q16(0.45)
        st.d_sigma = 0
        st.k_eff = Q16
        act = AgentAction("execute", {"cmd": goal})
        pol, msg = sv.check_autonomy(act, st)
        probe = float(int(st.sigma)) / float(Q16)
        cb_out, cb_msg = cb.check(probe, None)
        contained = ct.contain(goal, probe, None)
        raw["sovereign"] = sv.snapshot()
        raw["circuit"] = cb.snapshot()
        raw["config"] = base_cfg
        raw["last_goal"] = goal
        _save_sovereign_session(path, raw)
        print(
            json.dumps(
                {
                    "policy": pol,
                    "message": msg,
                    "circuit": cb_out,
                    "circuit_msg": cb_msg,
                    "containment": contained,
                    "sovereign": sv.snapshot(),
                },
                ensure_ascii=False,
                indent=2,
                default=str,
            )
        )
        return 0 if pol != "HALT" else 4

    print(
        "cos sovereign: use --goal STR [--simulate-steps N] [--max-actions K], "
        "or --status / --circuit, or --halt [--state-file PATH]",
        file=sys.stderr,
    )
    return 2


def _stub_resolve_results(task: str, nodes: List[str]) -> List[Dict[str, Any]]:
    import hashlib

    h = hashlib.sha256(task.encode("utf-8")).digest()[0]
    results: List[Dict[str, Any]] = []
    for i, nid in enumerate(nodes):
        sigma = 0.42 + float(i) * 0.02 + (h % 5) * 0.001
        results.append({"node": nid, "result": f"{nid}-claim-{i}", "sigma": min(0.95, sigma)})
    return results


def _cmd_resolve(args: argparse.Namespace) -> int:
    from cos.sigma_conflict import SigmaConflictResolver
    from cos.sigma_kernel_lock import KernelLock
    from cos.sigma_node_state import NodeStateMachine

    if getattr(args, "resolve_states", False):
        print(json.dumps({"node_state_machine": NodeStateMachine.STATES}, ensure_ascii=False, indent=2))
        return 0

    vt = str(getattr(args, "verify_trace", "") or "").strip()
    if vt:
        trace = [s.strip() for s in vt.split(",") if s.strip()]
        ok, msg = NodeStateMachine("verify").is_valid_trace(trace)
        print(json.dumps({"ok": ok, "message": msg}, ensure_ascii=False))
        return 0 if ok else 3

    if getattr(args, "kernel_lock_cmd", False):
        kl = KernelLock()
        demo = {"resolution": "SIGMA_MIN", "winner": "railo", "level": 1}
        out = kl.lock(demo, [])
        meta = {
            "invariants": kl.INVARIANTS,
            "kernel_hash": kl.compute_kernel_hash(),
            "demo_lock": out,
        }
        print(json.dumps(meta, ensure_ascii=False, indent=2, default=str))
        return 0

    task = str(getattr(args, "task", "") or "").strip()
    nodes_str = str(getattr(args, "nodes", "") or "").strip()
    if task and nodes_str:
        nodes = [x.strip() for x in nodes_str.split(",") if x.strip()]
        results = _stub_resolve_results(task, nodes)
        res = SigmaConflictResolver().resolve(results)
        lock = KernelLock().lock(res, results)
        print(
            json.dumps(
                {"task": task, "nodes": nodes, "results": results, "resolution": res, "kernel_lock": lock},
                ensure_ascii=False,
                indent=2,
                default=str,
            )
        )
        return 0

    print(
        "cos resolve: use --task STR --nodes a,b,c | --states | --verify-trace IDLE,ACTIVE,... | --kernel-lock",
        file=sys.stderr,
    )
    return 2


def _cmd_agent(args: argparse.Namespace) -> int:
    from cos.sigma_agent import AgentAction, SigmaAgent
    from cos.sigma_audit import SigmaAudit
    from cos.sigma_firewall import SigmaFirewall
    from cos.sigma_gate_core import SigmaState

    if getattr(args, "firewall", False):
        fw = SigmaFirewall()
        ok, sig, msg = fw.check_input(str(args.goal), _LabFirewallEncodeModel(), SigmaState())
        if not ok:
            print(
                json.dumps(
                    {"blocked": True, "layer": 1, "reason": msg, "sigma": float(sig)},
                    ensure_ascii=False,
                )
            )
            return 4

    if not args.mock:
        print(
            "cos agent: use --mock for a local σ-gated smoke (no live LLM). "
            "Wire SigmaAgent + your planner for full runs.",
            file=sys.stderr,
        )
        return 2

    class _EchoTool:
        def run(self, params: Dict[str, Any]) -> Any:
            return {"echo": params}

    class _MockModel:
        def __init__(self) -> None:
            self.k = 0

        def plan(self, goal: str, history: List[Dict[str, Any]]) -> AgentAction:
            _ = goal
            self.k += 1
            if self.k == 1:
                return AgentAction("grep", {"pattern": "TODO"})
            return AgentAction("rm", {"argv": "rm -rf /tmp/x"})

        def replan(self, goal: str, history: List[Dict[str, Any]], hint: str) -> AgentAction:
            _ = goal, history, hint
            return AgentAction("read", {"path": "/dev/null"})

    class _MockGate:
        def compute_sigma(self, model: Any, goal: str, action: AgentAction) -> float:
            _ = model, goal
            if action.tool_name == "rm" or "rm" in str(action.params).lower():
                return 0.4
            if "delete" in str(action.params).lower():
                return 0.8
            return 0.12

    tools: Dict[str, Any] = {"grep": _EchoTool(), "read": _EchoTool(), "rm": _EchoTool()}
    audit_path = Path(str(args.audit_dir)).expanduser()
    audit_path.mkdir(parents=True, exist_ok=True)
    audit = SigmaAudit(path=str(audit_path))
    ag = SigmaAgent(
        _MockModel(),
        _MockGate(),
        tools,
        allow_destructive=bool(args.allow_destructive),
        audit=audit,
    )
    hist = ag.run(str(args.goal), max_steps=int(args.max_steps))
    if args.json:
        # JSON-serializable view (AgentAction is not JSON default)
        serial = []
        for row in hist:
            a = row.get("action")
            serial.append(
                {
                    "step": row.get("step"),
                    "tool": getattr(a, "tool_name", None),
                    "params": getattr(a, "params", None),
                    "sigma": row.get("sigma"),
                    "verdict": row.get("verdict"),
                    "result": row.get("result"),
                }
            )
        print(json.dumps(serial, ensure_ascii=False))
        return 0
    print(json.dumps({"steps": len(hist), "last_verdict": hist[-1]["verdict"] if hist else None}, ensure_ascii=False))
    return 0


def _cmd_autonomous(args: argparse.Namespace) -> int:
    """σ-gated persistent loop lab (until CONVERGED / HALT / caps)."""
    from cos.autonomous import AutonomousAgent

    goal = str(getattr(args, "autonomous_goal", "") or "").strip()
    if not goal:
        print("cos autonomous: pass GOAL text", file=sys.stderr)
        return 2

    agent = AutonomousAgent(
        max_steps=int(getattr(args, "autonomous_max_steps", 20) or 20),
        timeout_s=float(getattr(args, "autonomous_timeout", 300) or 300),
        drift_threshold=float(getattr(args, "drift_threshold", 0.3) or 0.3),
    )
    agent.set_goal(goal)

    def _action(g: Any, ctx: str, step: int) -> str:
        _ = ctx
        return f"step {step} toward: {g}"

    result = agent.run(action_fn=_action, correct_fn=None)
    if result.get("error"):
        print(json.dumps(result, ensure_ascii=False), file=sys.stderr)
        return 1
    print(f"Result: {result['reason']} in {result['steps']} steps")
    print(f"σ: avg={result['avg_σ']} final={result['final_σ']}")
    print(f"Self-corrections: {result['self_corrections']}")
    return 0


def _cmd_omega(args: argparse.Namespace) -> int:
    from cos.omega import OmegaLoop, OmegaPhaseHarness

    if getattr(args, "omega_cognitive_step", False):
        goal = str(getattr(args, "goal", "") or "").strip() or "lab_step"
        oloop = OmegaLoop()
        _ = getattr(args, "mock", False)
        out = oloop.step(goal)
        if getattr(args, "json", False):
            print(json.dumps(out, ensure_ascii=False))
            return 0
        print(json.dumps({"mode": "cognitive_step", "step": out["step"], "verdict": out["verdict"], "σ": out["σ"]}, ensure_ascii=False))
        return 0

    if not str(getattr(args, "goal", "") or "").strip():
        print(
            "cos omega: pass --goal for 14-phase harness, or use --step (optional --goal)",
            file=sys.stderr,
        )
        return 1
    loop = OmegaPhaseHarness()
    if getattr(args, "mock", False):
        _ = loop  # reserved: swap mock backends in harness builds
    history = loop.run(str(args.goal), max_turns=int(args.turns))
    if getattr(args, "json", False):
        print(json.dumps(history, ensure_ascii=False))
        return 0
    n_ph = len(history[0]["phases"]) if history else 0
    last = history[-1] if history else {}
    print(
        json.dumps(
            {
                "turns": len(history),
                "phases_per_turn": n_ph,
                "last_continue": bool(last.get("continue", False)),
                "last_watchdog": int(last.get("watchdog", -1)),
            },
            ensure_ascii=False,
        )
    )
    return 0


def _load_trace_file(path: Path) -> List[Dict[str, Any]]:
    raw = path.read_text(encoding="utf-8")
    if path.suffix.lower() == ".jsonl":
        rows: List[Dict[str, Any]] = []
        for line in raw.splitlines():
            line = line.strip()
            if not line:
                continue
            obj = json.loads(line)
            if isinstance(obj, dict):
                rows.append(obj)
        return rows
    data = json.loads(raw)
    if isinstance(data, list):
        return [x for x in data if isinstance(x, dict)]
    if isinstance(data, dict):
        for key in ("steps", "traces", "history"):
            seq = data.get(key)
            if isinstance(seq, list):
                return [x for x in seq if isinstance(x, dict)]
    raise ValueError("trace file must be a JSON array, JSON object with steps/traces/history, or JSONL")


def _mock_drift_rows() -> List[Dict[str, Any]]:
    """Synthetic run: short-window σ rise → :class:`cos.sigma_alert.SigmaAlert` COHERENCE_DRIFT."""
    return [
        {"step": 0, "sigma": 0.1, "verdict": "ACCEPT", "signals": {"entropy": 0.2}},
        {"step": 1, "sigma": 0.1, "verdict": "ACCEPT", "signals": {"hide": 0.1}},
        {"step": 2, "sigma": 0.28, "verdict": "RETHINK", "signals": {"icr": 0.05}},
    ]


def _cmd_observe_traces(args: argparse.Namespace) -> int:
    from cos.sigma_alert import SigmaAlert
    from cos.sigma_observe import SigmaObserve

    rows: List[Dict[str, Any]] = []
    try:
        if getattr(args, "mock_drift", False):
            rows = _mock_drift_rows()
        elif getattr(args, "from_file", None):
            rows = _load_trace_file(Path(args.from_file).expanduser())
        else:
            print(
                "cos observe: pass --from-file PATH.json (or .jsonl) or --mock-drift",
                file=sys.stderr,
            )
            return 2
    except (OSError, ValueError, json.JSONDecodeError) as e:
        print(f"cos observe: failed to load traces: {e}", file=sys.stderr)
        return 2

    obs = SigmaObserve()
    traced = obs.trace(rows)
    payload: Dict[str, Any] = {}
    if getattr(args, "traces", False):
        payload["traces"] = traced
    if getattr(args, "alerts", False):
        payload["alerts"] = SigmaAlert(observe=obs).check(traced)
    if not payload:
        print("cos observe: pass at least one of --traces --alerts", file=sys.stderr)
        return 2
    print(json.dumps(payload, ensure_ascii=False, default=str))
    return 0


def _cmd_observe(args: argparse.Namespace) -> int:
    """Default: σ dashboard from today's JSONL; use --from-file / --mock-drift for legacy trace mode."""
    if getattr(args, "from_file", "") or getattr(args, "mock_drift", False):
        return _cmd_observe_traces(args)

    from cos.observe import SigmaObserve

    obs = SigmaObserve(log_dir=str(getattr(args, "observe_log_dir", "~/.cos/logs")))
    obs.load_today_jsonl()
    summary = obs.summary(last_n=getattr(args, "observe_last", 100))
    if getattr(args, "out_json", False):
        print(json.dumps(summary, ensure_ascii=False, default=str))
    else:
        print(f"Calls: {summary['count']}")
        print(f"σ avg: {summary.get('σ_avg', 'N/A')}")
        print(f"σ p95: {summary.get('σ_p95', 'N/A')}")
        print(f"Accept: {summary.get('accept_rate', 'N/A')}")
        print(f"Abstain: {summary.get('abstain_rate', 'N/A')}")
    return 0


def _cmd_drift(args: argparse.Namespace) -> int:
    from cos.drift import SigmaDrift

    try:
        base_raw = Path(args.drift_baseline).expanduser().read_text(encoding="utf-8")
        cur_raw = Path(args.drift_current).expanduser().read_text(encoding="utf-8")
        base = json.loads(base_raw)
        cur = json.loads(cur_raw)
    except (OSError, json.JSONDecodeError) as e:
        print(f"cos drift: {e}", file=sys.stderr)
        return 2
    if not isinstance(base, list) or not isinstance(cur, list):
        print("cos drift: --baseline and --current must be JSON arrays of σ values", file=sys.stderr)
        return 2
    d = SigmaDrift()
    d.set_baseline([float(x) for x in base])
    r = d.detect([float(x) for x in cur])
    if getattr(args, "out_json", False):
        print(json.dumps(r, ensure_ascii=False, default=str))
    else:
        print(
            f"drift={r['drift']} kl={r['kl_divergence']} alert={r['alert']} "
            f"baseline_mean={r.get('baseline_mean')} current_mean={r.get('current_mean')} — {r['reason']}",
        )
    return 0


def _cmd_monitor(args: argparse.Namespace) -> int:
    from cos.sigma_dashboard import SigmaDashboard
    from cos.sigma_observe import SigmaObserve

    if not getattr(args, "html", False):
        print(
            "cos monitor: use --html for σ dashboard HTML (see --from-file / --mock-drift).",
            file=sys.stderr,
        )
        return 2
    try:
        if getattr(args, "mock_drift", False):
            rows = _mock_drift_rows()
        elif getattr(args, "from_file", None):
            rows = _load_trace_file(Path(args.from_file).expanduser())
        else:
            print(
                "cos monitor: pass --from-file PATH.json or .jsonl, or --mock-drift",
                file=sys.stderr,
            )
            return 2
    except (OSError, ValueError, json.JSONDecodeError) as e:
        print(f"cos monitor: failed to load traces: {e}", file=sys.stderr)
        return 2

    traced = SigmaObserve().trace(rows)
    html_out = SigmaDashboard().generate_html(traced)
    out_path = getattr(args, "output", None)
    if out_path:
        p = Path(str(out_path)).expanduser()
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(html_out, encoding="utf-8")
        print(str(p))
        return 0
    print(html_out)
    return 0


def _cmd_federation(args: argparse.Namespace) -> int:
    import urllib.error
    import urllib.request

    ws = Path(getattr(args, "workspace", "") or "~/.cos/federation").expanduser()

    if getattr(args, "federated_aggregate", False):
        from cos.sigma_federated import demo_aggregate_memory

        ws.mkdir(parents=True, exist_ok=True)
        out = demo_aggregate_memory(include_poison=True, use_byzantine=True)
        (ws / "fed_v161_stats.json").write_text(
            json.dumps(out, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        print(json.dumps(out, ensure_ascii=False))
        return 0

    if getattr(args, "status", False):
        p = ws / "fed_lab_state.json"
        if p.is_file():
            print(p.read_text(encoding="utf-8"))
        else:
            print("{}")
        return 0

    if getattr(args, "train", False):
        from cos.sigma_federated import run_mock_federation_lab

        out = run_mock_federation_lab(
            rounds=int(args.rounds),
            workspace=str(ws),
            include_poison=not bool(getattr(args, "no_poison", False)),
            use_byzantine=not bool(getattr(args, "no_byzantine", False)),
        )
        print(json.dumps(out, ensure_ascii=False))
        return 0

    if getattr(args, "join", False):
        base = str(getattr(args, "server_url", "") or "").rstrip("/")
        if not base:
            print("cos federation --join requires --server-url", file=sys.stderr)
            return 2
        try:
            req = urllib.request.Request(
                base + "/join",
                data=json.dumps({"data_dir": str(getattr(args, "data", "") or "")}).encode("utf-8"),
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=8) as resp:
                body = resp.read().decode("utf-8", errors="replace")
        except (urllib.error.URLError, OSError, ValueError) as e:
            print(f"cos federation join: {e}", file=sys.stderr)
            return 3
        print(body)
        return 0

    if getattr(args, "server", False):
        from cos.sigma_federation_http import serve_federation_http

        host = str(getattr(args, "host", "127.0.0.1") or "127.0.0.1")
        port = int(args.port)
        httpd = serve_federation_http(host=host, port=port, workspace=str(ws))
        print(
            json.dumps({"listening": f"http://{host}:{port}", "workspace": str(ws)}, ensure_ascii=False),
            flush=True,
        )
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            httpd.shutdown()
        return 0

    print(
        "cos federation: use --train --rounds N, --status, --server --port P, or --join --server-url URL",
        file=sys.stderr,
    )
    return 2


def _cmd_exec(args: argparse.Namespace) -> int:
    from cos.sigma_digital_twin import SigmaDigitalTwin
    from cos.sigma_rollback import SigmaRollback
    from cos.sigma_sandbox import SigmaSandbox

    ws = Path(getattr(args, "workspace", "") or "~/.cos/exec").expanduser()

    if getattr(args, "rollback", False):
        rb = SigmaRollback(workspace=str(ws))
        restored = rb.rollback()
        print(
            json.dumps(
                {"restored": restored is not None, "state": restored},
                ensure_ascii=False,
                default=str,
            )
        )
        return 0

    n_sim = int(bool(getattr(args, "simulate", "")))
    n_sb = int(bool(getattr(args, "sandbox", "")))
    n_tw = int(bool(getattr(args, "twin", "")))
    if n_sim + n_sb + n_tw != 1:
        print(
            "cos exec: pass exactly one of --simulate CMD, --sandbox CMD, --twin CMD "
            "(or use --rollback alone)",
            file=sys.stderr,
        )
        return 2

    if getattr(args, "simulate", ""):
        twin = SigmaDigitalTwin()
        sim_out = twin.simulate(str(args.simulate), {})
        proceed, reason = twin.should_execute(sim_out)
        payload = {
            **sim_out,
            "should_execute": proceed,
            "reason": reason,
            "blocked": not proceed,
        }
        print(json.dumps(payload, ensure_ascii=False, default=str))
        return 0 if proceed else 4

    if getattr(args, "sandbox", ""):
        sb = SigmaSandbox()
        out = sb.execute(str(args.sandbox))
        print(json.dumps(out, ensure_ascii=False, default=str))
        return 3 if out.get("blocked") else 0

    twin = SigmaDigitalTwin()
    if getattr(args, "with_rollback", False):
        rb = SigmaRollback(workspace=str(ws))
        out = rb.execute_with_rollback(
            str(args.twin),
            twin,
            {},
            gap_threshold=float(args.gap_threshold),
        )
    else:
        out = twin.execute_with_twin(str(args.twin), {})
    print(json.dumps(out, ensure_ascii=False, default=str))
    return 0


def _cmd_simulate(args: argparse.Namespace) -> int:
    ns = argparse.Namespace(
        workspace=getattr(args, "workspace", "~/.cos/exec"),
        simulate=str(args.command),
        sandbox="",
        twin="",
        rollback=False,
        with_rollback=False,
        gap_threshold=0.3,
    )
    return _cmd_exec(ns)


def _cmd_prove(args: argparse.Namespace) -> int:
    import hashlib

    from cos.sigma_audit_chain import SigmaAuditChain
    from cos.sigma_gate_core import Q16, SigmaState, sigma_gate, sigma_q16, sigma_update
    from cos.sigma_zkp import SigmaZKReceipt

    z = SigmaZKReceipt()

    if getattr(args, "verify", ""):
        p = Path(str(args.verify)).expanduser()
        r = json.loads(p.read_text(encoding="utf-8"))
        ok, msg = z.verify_receipt(r)
        print(json.dumps({"ok": ok, "message": msg}, ensure_ascii=False))
        return 0 if ok else 3

    if getattr(args, "verify_chain", ""):
        p = Path(str(args.verify_chain)).expanduser()
        ch = SigmaAuditChain.from_jsonl(p)
        ok, msg = ch.verify_chain()
        if not ok:
            print(json.dumps({"ok": False, "message": msg}, ensure_ascii=False))
            return 3
        for i, row in enumerate(ch.chain):
            rok, rmsg = z.verify_receipt(row)
            if not rok:
                print(json.dumps({"ok": False, "row": i, "message": rmsg}, ensure_ascii=False))
                return 3
        print(json.dumps({"ok": True, "message": msg, "rows": len(ch.chain)}, ensure_ascii=False))
        return 0

    if getattr(args, "export_chain", False):
        src = Path(str(getattr(args, "chain_file", "") or "")).expanduser()
        out = Path(str(getattr(args, "output", "") or "")).expanduser()
        if not src.is_file() or not str(getattr(args, "output", "")):
            print("cos prove --export-chain requires --chain-file and --output", file=sys.stderr)
            return 2
        out.write_bytes(src.read_bytes())
        print(str(out))
        return 0

    prompt = str(getattr(args, "prompt", "") or "")
    response = str(getattr(args, "response", "") or "")
    if not prompt or not response or float(getattr(args, "sigma", -1.0)) < 0.0:
        print(
            "cos prove: use --prompt STR --response STR --sigma FLOAT "
            "or --verify FILE or --verify-chain FILE.jsonl or --export-chain …",
            file=sys.stderr,
        )
        return 2

    ph = hashlib.sha256(prompt.encode("utf-8")).hexdigest()
    rh = hashlib.sha256(response.encode("utf-8")).hexdigest()
    st = SigmaState()
    if not getattr(args, "no_warm", False):
        st.sigma = sigma_q16(0.99)
        st.d_sigma = 0
        st.k_eff = Q16
    sigma_update(st, float(args.sigma), float(getattr(args, "k_raw", 0.92)))
    verdict = sigma_gate(st)
    rec = z.create_receipt(st, verdict, ph, rh)
    outp = str(getattr(args, "out", "") or "").strip()
    if outp:
        op = Path(outp).expanduser()
        op.parent.mkdir(parents=True, exist_ok=True)
        op.write_text(json.dumps(rec, indent=2, ensure_ascii=False), encoding="utf-8")
        print(str(op))
    else:
        print(json.dumps(rec, ensure_ascii=False, indent=2))
    return 0


def _cmd_verify_commitment_lab(args: argparse.Namespace) -> int:
    """Lab SHA-256 commitment over a fixed σ-gate demo string pair (not a succinct zk-SNARK)."""

    from cos.sigma_gate import SigmaGate
    from cos.zkp import SigmaCommitment

    gate = SigmaGate()
    zkp = SigmaCommitment()
    demo_p, demo_r = "What is 2+2?", "4"
    sigma, verdict = gate.score(demo_p, demo_r)
    record = zkp.commit(demo_p, demo_r, sigma, verdict)
    if _cli_out_json(args):
        print(json.dumps(record, ensure_ascii=False))
        return 0
    print(zkp.proof_receipt(record))
    return 0


def _cmd_constitution(args: argparse.Namespace) -> int:
    from cos.sigma_gate import SigmaGate
    from cos.zkp import SigmaConstitution

    const = SigmaConstitution()
    gate = SigmaGate()
    result = const.check(gate)
    if _cli_out_json(args):
        print(json.dumps(result, ensure_ascii=False))
        return 0 if result["compliant"] else 3
    print(f"Compliant: {result['compliant']}")
    print(f"Rules: {result['rules']}")
    print(f"Hash: {result['constitution_hash'][:16]}...")
    if result["violations"]:
        for v in result["violations"]:
            print(f"  x {v}")
    else:
        print("  ok: all checked rules satisfied")
    return 0 if result["compliant"] else 3


def _cmd_space_check(args: argparse.Namespace) -> int:
    from cos.space_grade import SpaceGradeChecker

    checker = SpaceGradeChecker()
    path_s = str(getattr(args, "space_check_file", "") or "").strip()
    mod_dir = str(getattr(args, "space_check_module_dir", "") or "").strip()

    if path_s:
        result = checker.check_file(path_s)
        if result.get("error"):
            print(result["error"], file=sys.stderr)
            return 1
        rate = 1.0 if result.get("compliant") else 0.0
        print(f"Compliance: {rate}")
        for v in result.get("violations", [])[:50]:
            print(f"  Rule {v['rule']}: {v['description']} (line {v['line']})")
        return 0 if result.get("compliant") else 3

    base = mod_dir or str(Path(__file__).resolve().parent)
    result = checker.check_module(base)
    cr = result.get("compliance_rate", 0.0)
    print(
        f"Compliance rate: {cr} ({result.get('compliant')}/{result.get('total_files')} files clean)"
    )
    all_v = result.get("all_violations", [])
    for v in all_v[:20]:
        fn = v.get("file", "")
        print(
            f"  Rule {v['rule']}: {v['description']} ({fn}:{v['line']})"
        )
    if len(all_v) > 20:
        print(f"  … {len(all_v) - 20} more violations")
    return 0


def _cmd_mega(args: argparse.Namespace) -> int:
    """Default: :class:`~cos.mega.Mega` σ-sweep. With ``--fabric``, legacy Fabric demo."""

    if bool(getattr(args, "mega_fabric", False)):
        from cos.fabric import Fabric
        from cos.zkp import SigmaConstitution

        text = str(getattr(args, "mega_text", "") or "").strip()
        if not text:
            print("cos mega: INPUT text required (--fabric)", file=sys.stderr)
            return 2

        fab = Fabric()
        status = fab.boot()
        modules = status.get("modules", {})
        loaded = sum(1 for m in modules.values() if m.get("state") == "loaded")
        total = len(modules)
        layers = fab.layer_status()
        result = fab.process(text)
        const_result = SigmaConstitution().check(fab.gate)

        if _cli_out_json(args):
            print(
                json.dumps(
                    {
                        "boot": {"loaded": loaded, "total_modules": total},
                        "layer_status": layers,
                        "process": result,
                        "constitution": const_result,
                    },
                    ensure_ascii=False,
                    default=str,
                )
            )
            return 0

        print("\n╔══ CREATION OS MEGA (FABRIC) ══╗")
        print(f"║ Modules: {loaded}/{total} loaded ║")
        print("╚══════════════════════════════╝\n")

        for layer, info in sorted(layers.items()):
            coverage = int(float(info["coverage"]) * 100)
            filled = min(10, coverage // 10)
            bar = "█" * filled + "░" * (10 - filled)
            print(f"  {layer:15s} {bar} {coverage}%")

        print(f"\n[Processing: {text}]\n")
        sig = result.get("σ", result.get("sigma", 0.0))
        print(f"  σ      = {float(sig):.4f}")
        print(f"  σ_meta = {result.get('σ_meta', 'N/A')}")
        print(f"  Verdict: {result.get('verdict', 'N/A')}")
        print(f"  Latency: {result.get('latency_ms', 'N/A')}ms")
        print(f"  Layers:  {result.get('layers_active', 'N/A')}")

        trace = result.get("trace") or []
        if trace:
            print(f"\n  Trace ({len(trace)} steps):")
            for t in trace:
                layer = t.get("layer", "?")
                print(f"    {layer:15s} → {t}")

        c_line = "✓ compliant" if const_result["compliant"] else "✗ VIOLATION"
        print(f"\n  Constitution: {c_line}")

        print("\n  NOT AGI ACHIEVED")
        print("  σ-AWARE ARCHITECTURE")
        print("  1 = 1")
        return 0

    from cos.mega import Mega

    obs = str(getattr(args, "mega_text", "") or "").strip()
    if not obs:
        print("cos mega: OBSERVATION text required", file=sys.stderr)
        return 2
    goal = getattr(args, "mega_goal", None)
    if goal is not None:
        goal = str(goal).strip() or None
    cycles = max(1, int(getattr(args, "mega_cycles", 1) or 1))
    m = Mega()
    all_out: List[Dict[str, Any]] = []
    for _ in range(cycles):
        all_out.append(m.step(obs, goal))
    last = all_out[-1]

    if _cli_out_json(args):
        print(
            json.dumps(
                {
                    "cycles": cycles,
                    "last": last,
                    "status": m.status(),
                    "dream": m.dream() if cycles > 1 else None,
                },
                ensure_ascii=False,
                default=str,
            )
        )
        return 0

    for row in all_out:
        print(f"Cycle {row['cycle']}: σ_cycle={row.get('σ_cycle')}")
        for stage, data in row["stages"].items():
            if not isinstance(data, dict):
                continue
            marker = data.get("verdict", data.get("action", data.get("status", "")))
            σv = data.get("σ", data.get("σ_meta", ""))
            if σv != "" and marker != "":
                print(f"  {stage}: {marker} (σ={σv})")
            elif marker != "":
                print(f"  {stage}: {marker}")
            elif σv != "":
                print(f"  {stage}: σ={σv}")
    if cycles > 1:
        print()
        print(json.dumps(m.dream(), ensure_ascii=False, indent=2))
        print(f"\nStatus: {json.dumps(m.status(), ensure_ascii=False)}")
    print("\nNOT AGI ACHIEVED")
    return 0


def _cmd_mcp(args: argparse.Namespace) -> int:
    from cos.sigma_mcp_registry import SigmaMCPRegistry

    reg_path = Path(str(getattr(args, "registry", "") or "~/.cos/sigma_mcp_registry.json")).expanduser()
    reg = SigmaMCPRegistry.load(reg_path)
    did_any = False

    if getattr(args, "mcp_register", False):
        sid = str(getattr(args, "server_id", "") or "").strip()
        if not sid:
            print("cos mcp --register requires --server-id", file=sys.stderr)
            return 2
        surl = str(getattr(args, "server_url", "") or "").strip()
        meta: Dict[str, Any] = {}
        mj = str(getattr(args, "metadata_json", "") or "").strip()
        if mj:
            try:
                meta = json.loads(mj)
            except json.JSONDecodeError as e:
                print(f"cos mcp: invalid --metadata-json: {e}", file=sys.stderr)
                return 2
        reg.register(sid, surl, meta)
        reg.save(reg_path)
        print(json.dumps({"saved": str(reg_path), "id": sid}, ensure_ascii=False))
        did_any = True

    if getattr(args, "list_servers", False):
        if not reg.servers:
            print(json.dumps({"registry": str(reg_path), "servers": 0}, ensure_ascii=False))
        for line in reg.iter_lines():
            print(line)
        did_any = True

    if getattr(args, "wrap_hint", False):
        cmd = str(getattr(args, "wrap_server_cmd", "") or "").strip()
        if not cmd:
            print("cos mcp --wrap requires --server CMD", file=sys.stderr)
            return 2
        print(
            json.dumps(
                {
                    "spawn": cmd,
                    "python_module": "cos.sigma_mcp_middleware",
                    "note": (
                        "Wrap in-process tool handlers with SigmaMCPMiddleware.wrap_server or "
                        "wrap_tools_dict before exposing JSON-RPC. The native MCP binary remains "
                        "`creation_os_sigma_mcp` (see docs/MCP_COS_TOOLS.md)."
                    ),
                },
                ensure_ascii=False,
                indent=2,
            )
        )
        did_any = True

    if not did_any:
        try:
            from cos.mcp_server import run_server
        except ImportError as exc:
            print(f"cos mcp: {exc}", file=sys.stderr)
            return 1
        try:
            run_server(
                transport=str(getattr(args, "transport", "stdio")),
                host=str(getattr(args, "host", "127.0.0.1")),
                port=int(getattr(args, "port", 8000)),
            )
        except ImportError as exc:
            print(f"cos mcp: {exc}", file=sys.stderr)
            return 1
        return 0
    return 0


def _cmd_a2a(args: argparse.Namespace) -> int:
    from cos.sigma_a2a import SigmaA2A
    from cos.sigma_mcp_registry import SigmaMCPRegistry

    if not getattr(args, "a2a_send", False):
        print("cos a2a: use --send --from … --to … --message …", file=sys.stderr)
        return 2

    reg_path = Path(str(getattr(args, "registry", "") or "~/.cos/sigma_mcp_registry.json")).expanduser()
    reg = SigmaMCPRegistry.load(reg_path)
    fa = str(getattr(args, "from_agent", "") or "agent_1").strip() or "agent_1"
    ta = str(getattr(args, "to_agent", "") or "agent_2").strip() or "agent_2"
    if fa not in reg.servers:
        reg.register(fa, "", {"role": "sender"})
    if ta not in reg.servers:
        reg.register(ta, "", {"role": "receiver"})

    model = _LabFirewallEncodeModel()
    a2a = SigmaA2A(model=model, registry=reg)

    class _LocalPeer:
        def receive(self, message: Dict[str, Any]) -> Dict[str, Any]:
            return a2a.receive(message)

    msg_text = str(getattr(args, "message", "") or "")
    out = a2a.send(fa, _LocalPeer(), {"text": msg_text})
    reg.save(reg_path)
    print(json.dumps(out, ensure_ascii=False, default=str))
    if out.get("blocked"):
        return 4
    if out.get("accepted") is False:
        return 3
    return 0


def _ttt_default_state() -> Dict[str, Any]:
    return {
        "step": 0,
        "history": [],
        "living": {
            "application_updates": 0,
            "firmware_updates": 0,
            "kernel_pct": 12.0,
            "firmware_pct": 28.0,
            "application_pct": 60.0,
        },
    }


def _ttt_load(path: Path) -> Dict[str, Any]:
    if not path.is_file():
        return _ttt_default_state()
    try:
        with path.open("r", encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError):
        return _ttt_default_state()
    if not isinstance(data, dict):
        return _ttt_default_state()
    base = _ttt_default_state()
    base.update(data)
    if not isinstance(base.get("history"), list):
        base["history"] = []
    if not isinstance(base.get("living"), dict):
        base["living"] = _ttt_default_state()["living"]
    return base


def _ttt_save(path: Path, data: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write("\n")


def _cmd_ttt(args: argparse.Namespace) -> int:
    """σ-governed test-time training lab state (JSON under ``~/.cos`` by default)."""
    path = Path(str(getattr(args, "state_file", "") or "~/.cos/ttt_lab.json")).expanduser()
    st = _ttt_load(path)
    learn = bool(getattr(args, "learn", False))
    track = bool(getattr(args, "track", False))
    weights = bool(getattr(args, "weights", False))
    rollback = bool(getattr(args, "rollback", False))
    last_n = int(getattr(args, "last", 20) or 20)

    if rollback:
        hist = st.get("history", [])
        if not isinstance(hist, list) or not hist:
            print("cos ttt: nothing to rollback", file=sys.stderr)
            return 2
        last = hist.pop()
        tag = str(last.get("tag", ""))
        liv = st.setdefault("living", _ttt_default_state()["living"])
        if tag.startswith("RETHINK"):
            liv["application_updates"] = max(0, int(liv.get("application_updates", 0)) - 1)
        _ttt_save(path, st)
        print(json.dumps({"rolled_back": last}, ensure_ascii=False))
        return 0

    if weights:
        print(json.dumps({"living": st.get("living", {})}, ensure_ascii=False))
        return 0

    if track:
        from cos.sigma_adaptation import SigmaAdaptationTracker

        tr = SigmaAdaptationTracker()
        tr.history = [dict(x) for x in st.get("history", []) if isinstance(x, dict)]
        recent = tr.last_n(last_n)
        avg = 0.0
        if recent:
            avg = sum(float(x.get("delta", 0.0)) for x in recent) / float(len(recent))
        payload = {
            "last": len(recent),
            "avg_delta_sigma": round(avg, 6),
            "efficiency": round(tr.learning_efficiency(), 4),
            "should_continue": tr.should_continue_learning(window=10),
        }
        print(json.dumps(payload, ensure_ascii=False))
        return 0

    if learn:
        prompt = str(getattr(args, "prompt", "") or "").strip()
        if not prompt:
            print("cos ttt: --learn requires --prompt TEXT", file=sys.stderr)
            return 2
        try:
            import torch  # noqa: F401
        except ImportError:
            print("cos ttt: install torch for --learn", file=sys.stderr)
            return 2
        from cos.sigma_adaptation import SigmaAdaptationTracker
        from cos.sigma_ttt import build_sigma_gated_ttt_lab

        orch, _model, _gate = build_sigma_gated_ttt_lab()
        out, sigma, tag = orch.inference_with_learning(prompt, context=str(getattr(args, "context", "") or "") or None)
        st["step"] = int(st.get("step", 0)) + 1
        tr = SigmaAdaptationTracker()
        tr.history = [dict(x) for x in st.get("history", []) if isinstance(x, dict)]
        tr.track(int(st["step"]), float(orch.last_sigma_before), float(orch.last_sigma_after), tag)
        st["history"] = tr.history
        liv = st.setdefault("living", _ttt_default_state()["living"])
        if tag.startswith("RETHINK"):
            liv["application_updates"] = int(liv.get("application_updates", 0)) + 1
        if tag.startswith("ACCEPT"):
            liv["accept_events"] = int(liv.get("accept_events", 0)) + 1
        _ttt_save(path, st)
        print(
            json.dumps(
                {"output": out, "sigma": float(sigma), "tag": tag, "step": st["step"]},
                ensure_ascii=False,
            )
        )
        return 0

    print("cos ttt: pass --learn, --track, --weights, or --rollback (see --help)", file=sys.stderr)
    return 2


def _cmd_benchmark(args: argparse.Namespace) -> int:
    """Delegate to ``Makefile`` hardware / BitNet kernel lab targets."""
    if bool(getattr(args, "spike_vs_continuous", False)):
        from cos.sigma_spike import benchmark_spike_vs_continuous_mj_per_token

        print(json.dumps(benchmark_spike_vs_continuous_mj_per_token(), ensure_ascii=False))
        return 0

    root = Path(__file__).resolve().parents[2]
    make_cmd = ["make", "-C", str(root)]
    env = dict(os.environ)
    dim = int(getattr(args, "dim", 4096) or 4096)

    hybrid = bool(getattr(args, "hybrid", False))
    hybrid_ratio = bool(getattr(args, "hybrid_ratio", False))
    hybrid_memory = bool(getattr(args, "hybrid_memory", False))
    if hybrid_ratio or hybrid_memory:
        if not hybrid:
            print(
                "cos benchmark: --ratio and --memory require --hybrid (see cos benchmark --help)",
                file=sys.stderr,
            )
            return 2
    if hybrid:
        seq = str(getattr(args, "seq_len", "4096") or "4096").strip()
        if not seq:
            seq = "4096"
        env["COS_HYBRID_SEQ_LEN"] = seq
        mode = "all"
        if hybrid_ratio and not hybrid_memory:
            mode = "ratio"
        elif hybrid_memory and not hybrid_ratio:
            mode = "memory"
        env["COS_HYBRID_MODE"] = mode
        return int(subprocess.run(make_cmd + ["bench-hybrid"], env=env).returncode)

    hw_bsc = bool(
        getattr(args, "hardware", False)
        or getattr(args, "sigma_throughput", False)
        or getattr(args, "bsc_vs_gemm", False)
        or getattr(args, "bsc_simd", False)
        or getattr(args, "energy_per_verdict", False)
    )
    if hw_bsc and (dim < 64 or (dim % 64) != 0):
        print("cos benchmark: --dim must be >= 64 and a multiple of 64 for GEMM/BSC targets", file=sys.stderr)
        return 2

    bitnet_any = bool(
        getattr(args, "bitnet_sigma", False)
        or getattr(args, "bitnet_neon", False)
        or getattr(args, "cache_stats", False)
        or getattr(args, "early_exit", False)
        or getattr(args, "energy_per_token", False)
    )
    if bool(getattr(args, "bitnet_turbo", False)):
        env["CREATION_OS_GEMM_BSC_DIM"] = str(dim)
        r1 = subprocess.run(make_cmd + ["bench-bitnet-sigma"], env=env)
        r2 = subprocess.run(make_cmd + ["bench-sigma-perf"], env=env)
        return int(r1.returncode or r2.returncode)

    if bitnet_any:
        if bool(getattr(args, "energy_per_token", False)):
            print(
                "cos benchmark: energy_mj_per_token is not read in-tree; "
                "use a power-instrumented host and archive under benchmarks/bitnet/.",
                file=sys.stderr,
            )
        return int(subprocess.run(make_cmd + ["bench-bitnet-sigma"], env=env).returncode)

    if bool(getattr(args, "hardware", False)):
        env["CREATION_OS_GEMM_BSC_DIM"] = str(dim)
        return int(subprocess.run(make_cmd + ["bench-hardware"], env=env).returncode)

    if bool(getattr(args, "sigma_throughput", False)):
        return int(subprocess.run(make_cmd + ["bench-sigma-perf"], env=env).returncode)

    if bool(getattr(args, "bsc_vs_gemm", False) or bool(getattr(args, "bsc_simd", False))):
        env["CREATION_OS_GEMM_BSC_DIM"] = str(dim)
        return int(subprocess.run(make_cmd + ["bench-gemm-bsc"], env=env).returncode)

    if bool(getattr(args, "energy_per_verdict", False)):
        print(
            "cos benchmark: energy_j_per_verdict is not read in-tree; "
            "use a power-instrumented host and archive under benchmarks/hardware/.",
            file=sys.stderr,
        )
        return int(subprocess.run(make_cmd + ["bench-sigma-perf"], env=env).returncode)

    print(
        "cos benchmark: pass --hardware, --sigma-throughput, --bsc-vs-gemm, --bsc-simd, "
        "--energy-per-verdict, --bitnet-sigma, --bitnet-neon, --cache-stats, --early-exit, "
        "--energy-per-token, --bitnet-turbo, --hybrid (with optional --seq-len, --ratio, "
        "--memory), … (see --help)",
        file=sys.stderr,
    )
    return 2


def _parse_predict_actions_csv(s: str) -> List[str]:
    return [p.strip() for p in str(s).split(",") if p.strip()]


def _cmd_predict(args: argparse.Namespace) -> int:
    """σ-JEPA lab: latent predict (Ω PREDICT), roll-out imagination (Ω SIMULATE), or plan."""
    from cos.sigma_imagination import SigmaImagination
    from cos.sigma_jepa import LabLatentEncoder, LabLatentPredictor, SigmaJEPA

    dim = max(1, int(getattr(args, "dim", 8)))
    k_raw = float(getattr(args, "k_raw", 0.92))
    enc = LabLatentEncoder(dim=dim)
    pred = LabLatentPredictor(drift=float(getattr(args, "drift", 0.02)))
    jepa = SigmaJEPA(enc, pred, None, k_raw=k_raw)
    imagination = SigmaImagination(jepa, k_raw=k_raw)

    def _zvec(z: Any) -> List[float]:
        return [round(float(x), 6) for x in z]

    if bool(getattr(args, "plan", False)):
        path = getattr(args, "candidates", None)
        if not path:
            print("cos predict --plan: requires --candidates PATH.jsonl", file=sys.stderr)
            return 2
        horizon = max(1, int(getattr(args, "horizon", 5)))
        candidates: List[List[str]] = []
        with Path(str(path)).open("r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    obj = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if not isinstance(obj, dict):
                    continue
                seq = obj.get("actions")
                if seq is None:
                    seq = obj.get("plan")
                if isinstance(seq, list):
                    candidates.append([str(x) for x in seq])
        if not candidates:
            print("cos predict --plan: no valid candidate rows (need actions array per line)", file=sys.stderr)
            return 2
        best_plan, best_sigma = imagination.plan("", candidate_actions=candidates, horizon=horizon)
        out = {
            "best_plan": list(best_plan) if best_plan is not None else [],
            "best_sigma": float(best_sigma),
            "horizon": horizon,
        }
        print(json.dumps(out, ensure_ascii=False))
        return 0

    if bool(getattr(args, "imagine", False)):
        obs = str(getattr(args, "observation", "") or "lab")
        horizon = max(1, int(getattr(args, "horizon", 10)))
        actions = _parse_predict_actions_csv(str(getattr(args, "actions", "") or ""))
        if not actions:
            actions = ["noop"] * horizon
        result = imagination.imagine(obs, actions, max_steps=horizon)
        result["trajectory"] = [
            {k: v for k, v in row.items() if k != "z"} for row in result["trajectory"]
        ]
        print(json.dumps(result, ensure_ascii=False))
        return 0

    obs = str(getattr(args, "observation", "") or "")
    if not obs.strip():
        print(
            "cos predict: pass --observation TEXT (or use --imagine / --plan); "
            "optional --action, --next-observation for σ_model.",
            file=sys.stderr,
        )
        return 2
    action = getattr(args, "action", None)
    step = jepa.world_model_step(obs, action=str(action) if action else None)
    next_obs = getattr(args, "next_observation", None)
    payload: Dict[str, Any] = {
        "z_current": _zvec(step["z_current"]),
        "z_predicted": _zvec(step["z_predicted"]),
        "ready_to_measure": bool(step.get("ready_to_measure")),
    }
    if next_obs is not None and str(next_obs).strip():
        upd = jepa.update_on_observation(step, str(next_obs))
        payload["sigma_model"] = float(upd["sigma_model"])
        payload["verdict"] = upd["verdict"].name if hasattr(upd["verdict"], "name") else str(upd["verdict"])
        payload["world_model_quality"] = float(upd["world_model_quality"])
    print(json.dumps(payload, ensure_ascii=False))
    return 0


def _cmd_interpret(args: argparse.Namespace) -> int:
    """σ-SAE interpretability lab (v121 scaffold): JSON on stdout."""
    from cos.sigma_cascade import cascade_summary

    def _mock_block(op: str) -> Dict[str, Any]:
        cs = cascade_summary()
        base: Dict[str, Any] = {"cmd": "interpret", "op": op, "cascade": cs}
        if op == "decompose":
            base.update({"sigma_sae": 0.25, "clarify_score": 0.4, "recommend_abstain": False})
        elif op == "steer":
            base.update({"sigma_sae": 0.22, "clarify_score": 0.35})
        elif op == "steer-permanent":
            base.update({"patch": None, "hits": 1})
        elif op == "feature-map":
            base.update({"metrics": [{"index": 0, "interpretability": 0.9, "steerability": 0.1}]})
        elif op == "explain":
            base.update({"active_labels": ["mock"], "hallucination_hit": False})
        elif op == "correlate":
            base.update({"correlations": [{"feature_index": 0, "pearson_r": 0.5}]})
        return base

    op = None
    if bool(getattr(args, "decompose", False)):
        op = "decompose"
    elif bool(getattr(args, "steer", False)):
        op = "steer"
    elif bool(getattr(args, "steer_permanent", False)):
        op = "steer-permanent"
    elif bool(getattr(args, "feature_map", False)):
        op = "feature-map"
    elif bool(getattr(args, "explain", False)):
        op = "explain"
    elif bool(getattr(args, "correlate", False)):
        op = "correlate"
    if op is None:
        print("cos interpret: pick one mode flag", file=sys.stderr)
        return 2

    if bool(getattr(args, "mock", False)):
        print(json.dumps(_mock_block(op), ensure_ascii=False))
        return 0

    import torch
    import torch.nn as nn

    from cos.sigma_gate_sae_signal import compute_l5_sae_signal
    from cos.sigma_sae import SigmaSAE
    from cos.sigma_steering import SigmaSteering

    class _ToySAE(nn.Module):
        def __init__(self, d_in: int, d_dict: int) -> None:
            super().__init__()
            self.enc = nn.Linear(d_in, d_dict, bias=True)
            self.dec = nn.Linear(d_dict, d_in, bias=True)

        def encode(self, h: torch.Tensor) -> torch.Tensor:
            return torch.relu(self.enc(h))

        def decode(self, z: torch.Tensor) -> torch.Tensor:
            return self.dec(z)

    d_dict = max(2, int(getattr(args, "dict_dim", 16)))
    hpath = str(getattr(args, "hidden_json", "") or "").strip()
    if hpath:
        raw = json.loads(Path(hpath).read_text(encoding="utf-8"))
        if not isinstance(raw, list):
            print("cos interpret: --hidden-json must be a JSON array of floats", file=sys.stderr)
            return 2
        vec = [float(x) for x in raw]
        dim = len(vec)
        h = torch.tensor(vec, dtype=torch.float32)
    else:
        dim = max(1, int(getattr(args, "dim", 8)))
        g = torch.Generator()
        g.manual_seed(0)
        h = torch.randn(dim, generator=g)

    hf_raw = str(getattr(args, "halluc_features", "") or "")
    known = tuple(int(x.strip()) for x in hf_raw.split(",") if x.strip().isdigit())
    sae = SigmaSAE(
        _ToySAE(dim, d_dict),
        known_hallucination_features=known,
        activation_threshold=float(getattr(args, "activation_threshold", 0.1)),
    )
    labels = [s.strip() for s in str(getattr(args, "labels", "") or "").split(",") if s.strip()]
    while len(labels) < d_dict:
        labels.append(f"f{len(labels)}")
    ctau = float(getattr(args, "clarify_tau", 0.62))

    if op == "correlate":
        sp = str(getattr(args, "sigma_json", "") or "").strip()
        ap = str(getattr(args, "activations_json", "") or "").strip()
        if not sp or not ap:
            print("cos interpret --correlate needs --sigma-json and --activations-json", file=sys.stderr)
            return 2
        xs = json.loads(Path(sp).read_text(encoding="utf-8"))
        mat = json.loads(Path(ap).read_text(encoding="utf-8"))
        if not isinstance(xs, list) or not isinstance(mat, list):
            print("cos interpret: sigma-json list; activations-json list of rows", file=sys.stderr)
            return 2
        corr = SigmaSteering().correlate_sigma_with_activations(xs, mat)
        print(
            json.dumps(
                {"cmd": "interpret", "op": "correlate", "cascade": cascade_summary(), "correlations": corr},
                ensure_ascii=False,
            )
        )
        return 0

    if op == "steer-permanent":
        st = SigmaSteering()
        feat_idx = int(getattr(args, "feature_idx", 0))
        for _ in range(max(1, int(getattr(args, "replay", 1)))):
            st.record_hallucination_hit(feat_idx)
        patch = st.suggest_permanent_patch(
            feat_idx,
            min_hits=int(getattr(args, "min_hits", 3)),
            alpha_crit=float(getattr(args, "alpha_crit", 0.15)),
            sigma_drop_if_ablated=float(getattr(args, "sigma_drop", 0.25)),
        )
        print(
            json.dumps(
                {"cmd": "interpret", "op": "steer-permanent", "cascade": cascade_summary(), "patch": patch},
                ensure_ascii=False,
            )
        )
        return 0

    if op == "steer":
        st = SigmaSteering()
        fj = str(getattr(args, "faithful_json", "") or "").strip()
        hj = str(getattr(args, "halluc_json", "") or "").strip()
        uf = None
        uh = None
        if fj:
            u = json.loads(Path(fj).read_text(encoding="utf-8"))
            if not isinstance(u, list):
                print("cos interpret: faithful-json must be a JSON array", file=sys.stderr)
                return 2
            uf = torch.tensor([float(x) for x in u], dtype=torch.float32)
        if hj:
            u = json.loads(Path(hj).read_text(encoding="utf-8"))
            if not isinstance(u, list):
                print("cos interpret: halluc-json must be a JSON array", file=sys.stderr)
                return 2
            uh = torch.tensor([float(x) for x in u], dtype=torch.float32)
        bundle = st.steer_with_sigma_sae(
            sae,
            h,
            faithful_direction=uf,
            hallucinatory_direction=uh,
            scale_faithful=float(getattr(args, "scale_faithful", 0.05)),
            scale_halluc_down=float(getattr(args, "scale_halluc_down", 0.1)),
        )
        print(
            json.dumps(
                {
                    "cmd": "interpret",
                    "op": "steer",
                    "cascade": cascade_summary(),
                    "sigma_sae": bundle["sigma_sae"],
                    "meta": bundle["meta"],
                    "clarify": bundle["clarify"],
                    "l5": compute_l5_sae_signal(sae, bundle["hidden_steered"], clarify_tau=ctau, feature_labels=labels),
                },
                ensure_ascii=False,
            )
        )
        return 0

    if op == "decompose":
        agg, feat, meta = sae.aggregate_sigma_sae(h)
        dom, shares = sae.decompose_dominance_sigma(feat)
        clar = sae.clarify_scores(feat)
        prune = sae.plan_cb_sae_prune(
            feat,
            tau_interp=float(getattr(args, "tau_interp", 0.2)),
            tau_steer=float(getattr(args, "tau_steer", 0.2)),
        )
        out = {
            "cmd": "interpret",
            "op": "decompose",
            "cascade": cascade_summary(),
            "sigma_sae": float(agg),
            "meta": meta,
            "dominance": float(dom),
            "top_shares": shares[: min(32, len(shares))],
            "clarify": clar,
            "cb_sae_prune_plan": prune,
            "l5": compute_l5_sae_signal(sae, h, clarify_tau=ctau, feature_labels=labels),
        }
        print(json.dumps(out, ensure_ascii=False))
        return 0

    if op == "feature-map":
        _, feat, _ = sae.aggregate_sigma_sae(h)
        rows = sae.cb_sae_feature_metrics(feat)
        out = {
            "cmd": "interpret",
            "op": "feature-map",
            "cascade": cascade_summary(),
            "metrics": rows,
            "prune": sae.plan_cb_sae_prune(
                feat,
                tau_interp=float(getattr(args, "tau_interp", 0.2)),
                tau_steer=float(getattr(args, "tau_steer", 0.2)),
            ),
            "l5": compute_l5_sae_signal(sae, h, clarify_tau=ctau, feature_labels=labels),
        }
        print(json.dumps(out, ensure_ascii=False))
        return 0

    if op == "explain":
        _, feat, _ = sae.aggregate_sigma_sae(h)
        exp = sae.explain_sigma(feat, labels)
        hit, idx = sae.detect_hallucination_features(feat)
        out = {
            "cmd": "interpret",
            "op": "explain",
            "cascade": cascade_summary(),
            "active_labels": exp,
            "hallucination_hit": hit,
            "hallucination_indices": idx,
            "l5": compute_l5_sae_signal(sae, h, clarify_tau=ctau, feature_labels=labels),
        }
        print(json.dumps(out, ensure_ascii=False))
        return 0

    return 2


def _gap_lab_twin_bundle() -> Tuple[Any, Any]:
    from cos.sigma_twin import TwinGateLab, TwinModelLab

    return TwinGateLab(), TwinModelLab(style="default")


def _fewshot_state_path(args: argparse.Namespace) -> Path:
    return Path(str(getattr(args, "fewshot_state", "") or "~/.cos/sigma_fewshot_lab.json")).expanduser()


def _symbolic_state_path(args: argparse.Namespace) -> Path:
    return Path(str(getattr(args, "symbolic_state", "") or "~/.cos/sigma_symbolic_lab.json")).expanduser()


def _cmd_fewshot(args: argparse.Namespace) -> int:
    import json as _json

    from cos.sigma_fewshot import HashEmbeddingEncoder, SigmaFewShot

    path = _fewshot_state_path(args)
    gate, model = _gap_lab_twin_bundle()
    fs = SigmaFewShot(gate, model, HashEmbeddingEncoder())
    if path.is_file():
        try:
            fs.import_state(_json.loads(path.read_text(encoding="utf-8")))
        except (_json.JSONDecodeError, OSError, TypeError, ValueError):
            print(f"cos fewshot: ignoring unreadable state file {path}", file=sys.stderr)

    if getattr(args, "fewshot_clear", False):
        fs.prototypes.clear()
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(_json.dumps(fs.export_state(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(_json.dumps({"cmd": "fewshot", "cleared": True}, ensure_ascii=False))
        return 0

    def _persist() -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(_json.dumps(fs.export_state(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    if getattr(args, "fewshot_learn", False):
        task = str(getattr(args, "fewshot_task", "") or "").strip()
        raw = str(getattr(args, "fewshot_examples", "") or "").strip()
        if not task or not raw:
            print("cos fewshot --learn requires --task and --examples JSON", file=sys.stderr)
            return 2
        try:
            examples = _json.loads(raw)
        except _json.JSONDecodeError as e:
            print(f"cos fewshot: invalid --examples JSON: {e}", file=sys.stderr)
            return 2
        if not isinstance(examples, list):
            print("cos fewshot: --examples must be a JSON array of pairs", file=sys.stderr)
            return 2
        pairs: List[Tuple[str, str]] = []
        for row in examples:
            if isinstance(row, (list, tuple)) and len(row) >= 2:
                pairs.append((str(row[0]), str(row[1])))
        if not pairs:
            print("cos fewshot: need at least one [input, output] pair", file=sys.stderr)
            return 2
        out = fs.learn(task, pairs)
        _persist()
        print(_json.dumps({"cmd": "fewshot", **out}, ensure_ascii=False, indent=2))
        return 0

    if getattr(args, "fewshot_predict", False):
        task = str(getattr(args, "fewshot_task", "") or "").strip()
        q = str(getattr(args, "fewshot_query", "") or "").strip()
        if not task or not q:
            print("cos fewshot --predict requires --task and --query", file=sys.stderr)
            return 2
        out = fs.predict(task, q)
        print(_json.dumps({"cmd": "fewshot", **out}, ensure_ascii=False, indent=2, default=str))
        return 0 if "error" not in out else 3

    if getattr(args, "fewshot_adapt", False):
        task = str(getattr(args, "fewshot_task", "") or "").strip()
        raw = str(getattr(args, "fewshot_example", "") or "").strip()
        if not task or not raw:
            print("cos fewshot --adapt requires --task and --example JSON pair", file=sys.stderr)
            return 2
        try:
            pair = _json.loads(raw)
        except _json.JSONDecodeError as e:
            print(f"cos fewshot: invalid --example: {e}", file=sys.stderr)
            return 2
        if not isinstance(pair, (list, tuple)) or len(pair) < 2:
            print("cos fewshot: --example must be [input, output]", file=sys.stderr)
            return 2
        out = fs.adapt(task, (str(pair[0]), str(pair[1])))
        _persist()
        print(_json.dumps({"cmd": "fewshot", **out}, ensure_ascii=False, indent=2))
        return 0

    print("cos fewshot: use --learn | --predict | --adapt | --clear", file=sys.stderr)
    return 2


def _cmd_symbolic(args: argparse.Namespace) -> int:
    import json as _json

    from cos.sigma_symbolic import SigmaSymbolic, parse_goal, parse_rule

    path = _symbolic_state_path(args)
    gate, _model = _gap_lab_twin_bundle()
    sym = SigmaSymbolic(gate)
    if path.is_file():
        try:
            sym.from_dict(_json.loads(path.read_text(encoding="utf-8")))
        except (_json.JSONDecodeError, OSError, TypeError, ValueError):
            print(f"cos symbolic: ignoring unreadable state file {path}", file=sys.stderr)

    def _persist_sym() -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(_json.dumps(sym.to_dict(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    if getattr(args, "symbolic_clear", False):
        sym.facts.clear()
        sym.rules.clear()
        _persist_sym()
        print(_json.dumps({"cmd": "symbolic", "cleared": True}, ensure_ascii=False))
        return 0

    s_assert = str(getattr(args, "symbolic_assert", "") or "").strip()
    if s_assert:
        try:
            sym.assert_fact_goal(parse_goal(s_assert))
        except ValueError as e:
            print(f"cos symbolic: bad --assert: {e}", file=sys.stderr)
            return 2
        _persist_sym()
        print(_json.dumps({"cmd": "symbolic", "asserted": s_assert}, ensure_ascii=False))
        return 0

    s_rule = str(getattr(args, "symbolic_rule", "") or "").strip()
    if s_rule:
        try:
            sym.assert_rule_dict(parse_rule(s_rule))
        except ValueError as e:
            print(f"cos symbolic: bad --rule: {e}", file=sys.stderr)
            return 2
        _persist_sym()
        print(_json.dumps({"cmd": "symbolic", "rule": s_rule}, ensure_ascii=False))
        return 0

    s_query = str(getattr(args, "symbolic_query", "") or "").strip()
    if s_query:
        try:
            sols = sym.query(s_query)
        except ValueError as e:
            print(f"cos symbolic: bad --query: {e}", file=sys.stderr)
            return 2
        print(_json.dumps({"cmd": "symbolic", "query": s_query, "solutions": sols}, ensure_ascii=False, indent=2))
        return 0

    if getattr(args, "symbolic_resolve_demo", False):
        c_neg = SigmaSymbolic.literal("human", ["X"], neg=True)
        c_pos_m = SigmaSymbolic.literal("mortal", ["X"], neg=False)
        c_a = [c_neg, c_pos_m]
        c_b = [SigmaSymbolic.literal("human", ["socrates"], neg=False)]
        r = sym.resolution(c_a, c_b)
        print(_json.dumps({"cmd": "symbolic", "resolution_demo": r}, ensure_ascii=False, indent=2, default=str))
        return 0

    ca = str(getattr(args, "symbolic_clause_a", "") or "").strip()
    cb = str(getattr(args, "symbolic_clause_b", "") or "").strip()
    if ca and cb:
        try:
            la = _json.loads(ca)
            lb = _json.loads(cb)
            if not isinstance(la, list) or not isinstance(lb, list):
                raise ValueError("clauses must be JSON arrays")
            r = sym.resolution(la, lb)
        except (ValueError, _json.JSONDecodeError, TypeError) as e:
            print(f"cos symbolic: --clause-a/b: {e}", file=sys.stderr)
            return 2
        print(_json.dumps({"cmd": "symbolic", "resolution": r}, ensure_ascii=False, indent=2, default=str))
        return 0

    print(
        "cos symbolic: use --assert, --rule, --query, --resolve-demo, or --clause-a/--clause-b JSON",
        file=sys.stderr,
    )
    return 2


def _cmd_gate_score(args: argparse.Namespace) -> int:
    from cos import SigmaGate

    gate = SigmaGate()
    sigma, verdict = gate.score(str(args.prompt), str(args.response))
    if _cli_verbose(args):
        print(
            f"[cos] lite_mode={gate._mode!r} threshold_accept={gate.threshold_accept} "
            f"threshold_abstain={gate.threshold_abstain}",
            file=sys.stderr,
        )
    if _cli_out_json(args):
        print(json.dumps({"sigma": float(sigma), "verdict": str(verdict)}, ensure_ascii=False))
        return 2 if verdict == "ABSTAIN" else 0
    print(f"σ={sigma:.4f} {verdict}")
    return 2 if verdict == "ABSTAIN" else 0


def _cmd_reason_cli(args: argparse.Namespace) -> int:
    from cos.symbolic import SigmaSymbolic

    eng = SigmaSymbolic()
    for raw in getattr(args, "reason_facts", None) or []:
        parts = str(raw).split(":")
        pred = parts[0].strip()
        if not pred:
            print("cos reason: empty predicate in --facts entry", file=sys.stderr)
            return 1
        eng.add_fact(pred, *parts[1:])
    q = str(getattr(args, "reason_query", "") or "").strip()
    if not q:
        print("cos reason: pass --query predicate:arg1:arg2 (colon-separated)", file=sys.stderr)
        return 1
    qparts = q.split(":")
    out = eng.query(qparts[0], *qparts[1:])
    if _cli_out_json(args):
        payload = {
            "solutions": [{k: str(v) for k, v in sol.items()} for sol in out["solutions"]],
            "trace": out["trace"],
            "proof_sigma": eng.σ_proof(out["trace"]),
        }
        print(json.dumps(payload, ensure_ascii=False))
        return 0
    for sol in out["solutions"]:
        print("  Solution:", {k: str(v) for k, v in sol.items()})
    print(f"  Proof σ: {eng.σ_proof(out['trace']):.3f}")
    return 0


def _cmd_cos_version(args: argparse.Namespace) -> int:
    from cos import __version__

    if _cli_out_json(args):
        print(json.dumps({"package": "creation-os", "version": __version__}, ensure_ascii=False))
        return 0
    print(f"creation-os {__version__}")
    return 0


def _cmd_init(args: argparse.Namespace) -> int:
    from cos.init import run_cos_init

    dest = Path(str(getattr(args, "init_dest", ".")))
    return run_cos_init(
        name=str(getattr(args, "init_name")),
        persona=str(getattr(args, "init_persona", "enterprise")),
        dest=dest,
    )


def _bench_resume(args: argparse.Namespace) -> int:
    from cos import SigmaGate
    from cos.eval.checkpoint_eval import CheckpointEval, load_checkpoint_meta
    from cos.eval.multi_model_eval import (
        MultiModelEval,
        compute_metrics_from_result_rows,
        load_eval_tuples,
    )

    ckpt = Path(str(getattr(args, "bench_resume", "") or "").strip())
    if not ckpt.is_file():
        print(f"cos bench: checkpoint not found: {ckpt}", file=sys.stderr)
        return 1
    meta = load_checkpoint_meta(ckpt)
    if not meta.get("model_key") or not meta.get("benchmark"):
        print(
            "cos bench: checkpoint meta missing model_key/benchmark (regenerate with --multi-model).",
            file=sys.stderr,
        )
        return 1
    gate = SigmaGate()
    benchmark = str(meta["benchmark"])
    model_key = str(meta["model_key"])
    endpoint = str(meta.get("endpoint", "http://127.0.0.1:8000/v1"))
    n = int(meta.get("n", getattr(args, "bench_n", 30) or 30))
    ck_every = int(meta.get("checkpoint_every", getattr(args, "bench_checkpoint_every", 5) or 5))
    out_dir = meta.get("output_dir", getattr(args, "bench_output_dir", "eval_results") or "eval_results")
    data_path_raw = meta.get("data_path") or str(getattr(args, "bench_data_path", "") or "").strip()
    dp = Path(data_path_raw) if data_path_raw else None
    pairs = load_eval_tuples(benchmark, data_path=dp)
    me = MultiModelEval(gate, endpoint)

    def eval_fn(i: int, pair: tuple[str, str]):
        prompt, gold = pair
        response = me._generate(model_key, prompt)
        sigma, verdict = gate.score(prompt, response)
        correct = MultiModelEval._check_correct(response, gold)
        return {
            "prompt": prompt[:2000],
            "response": response[:4000],
            "gold": gold[:500],
            "sigma": float(sigma),
            "verdict": str(verdict),
            "correct": bool(correct),
            "model": model_key,
            "benchmark": benchmark,
            "index": i,
        }

    ck_ob = CheckpointEval(out_dir)
    rows = ck_ob.continue_from_path(ckpt, eval_fn, pairs, n, checkpoint_every=ck_every)
    metrics = compute_metrics_from_result_rows(rows)
    metrics.update({"model": model_key, "benchmark": benchmark, "rows": rows, "resumed": True})
    if _cli_out_json(args):
        print(json.dumps(metrics, ensure_ascii=False, indent=2))
        return 0
    print(
        f"resumed model={model_key} benchmark={benchmark} n_done={metrics.get('n')} "
        f"AUROC={metrics.get('auroc')} abstain={metrics.get('abstention_rate')}"
    )
    return 0


def _bench_multi_model(args: argparse.Namespace) -> int:
    import hashlib
    import time

    from cos import SigmaGate
    from cos.eval.multi_model_eval import (
        EVAL_MODELS,
        format_r_ml_markdown_table,
        run_one_model_checkpointed,
    )

    raw_models = str(getattr(args, "bench_models_list", "") or "").strip()
    if not raw_models:
        print("cos bench: --multi-model requires --models key1,key2 (see eval/multi_model_eval.py)", file=sys.stderr)
        return 1
    models = [m.strip() for m in raw_models.split(",") if m.strip()]
    for m in models:
        if m not in EVAL_MODELS:
            print(
                f"cos bench: unknown model {m!r}; known: {', '.join(sorted(EVAL_MODELS))}",
                file=sys.stderr,
            )
            return 1
    bench_raw = str(getattr(args, "bench_dataset", "") or "").strip().lower()
    if not bench_raw:
        print("cos bench: --multi-model requires --dataset truthfulqa[,simpleqa,…]", file=sys.stderr)
        return 1
    benchmarks = [b.strip() for b in bench_raw.split(",") if b.strip()]
    n = int(getattr(args, "bench_n", 30) or 30)
    endpoint = str(getattr(args, "bench_endpoint", "http://127.0.0.1:8000/v1") or "http://127.0.0.1:8000/v1")
    ck_every = int(getattr(args, "bench_checkpoint_every", 5) or 5)
    out_dir = Path(str(getattr(args, "bench_output_dir", "eval_results") or "eval_results"))
    dpath = str(getattr(args, "bench_data_path", "") or "").strip()
    data_path = Path(dpath) if dpath else None
    run_id = hashlib.sha256(str(time.time()).encode()).hexdigest()[:8]
    gate = SigmaGate()
    flat: List[Dict[str, Any]] = []
    payload_models: Dict[str, Any] = {}

    def _prog(cur: int, total: int, _row: Mapping[str, Any]) -> None:
        if _cli_verbose(args):
            print(f"  [{cur}/{total}] checkpoint", file=sys.stderr)

    for model in models:
        for bmark in benchmarks:
            try:
                out = run_one_model_checkpointed(
                    gate,
                    model_key=model,
                    benchmark=bmark,
                    n=n,
                    endpoint=endpoint,
                    output_dir=out_dir,
                    checkpoint_every=ck_every,
                    run_id=run_id,
                    data_path=data_path,
                    progress=_prog if _cli_verbose(args) else None,
                )
            except Exception as exc:
                payload_models[f"{model}:{bmark}"] = {"error": str(exc), "model": model, "benchmark": bmark}
                flat.append(
                    {
                        "model": model,
                        "benchmark": bmark,
                        "error": str(exc),
                        "status": "—",
                    }
                )
                continue
            payload_models[f"{model}:{bmark}"] = out
            flat.append(
                {
                    "model": model,
                    "auroc": out.get("auroc"),
                    "abstention_rate": out.get("abstention_rate"),
                    "smece": out.get("smece"),
                    "snr": out.get("snr"),
                    "benchmark": bmark,
                    "status": out.get("status", "—"),
                    "checkpoint_file": out.get("checkpoint_file"),
                }
            )

    md = format_r_ml_markdown_table(flat, include_known_halu=True)
    bundle = {
        "version": "multi_model_eval_v1",
        "not_agi_achieved": True,
        "run_id": run_id,
        "n": n,
        "models": payload_models,
        "flat": flat,
        "markdown": md,
    }
    if _cli_out_json(args):
        print(json.dumps(bundle, ensure_ascii=False, indent=2))
        return 0
    print(md)
    return 0


def _fmt_mtier_text(payload: dict) -> str:
    from cos.report import SigmaReport

    rows = payload.get("rows") or []
    return SigmaReport().mtier_table_markdown(rows)


def _bench_sigma_harness(args: argparse.Namespace) -> int:
    """σ-gate eval on bundled (prompt, response, gold) tuples — no external dataset fetch."""
    from pathlib import Path

    from cos.eval.harness import SigmaHarness, default_harness_dataset

    ds_name = str(getattr(args, "bench_dataset") or "TruthfulQA").strip() or "TruthfulQA"
    dataset = default_harness_dataset(ds_name)
    safe = ds_name.replace(" ", "_").lower()
    out_dir = Path(str(getattr(args, "bench_output_dir") or "eval_results"))
    harness = SigmaHarness(output_dir=out_dir)
    bn = int(getattr(args, "bench_n") or len(dataset))
    ck_every = int(getattr(args, "bench_checkpoint_every") or 5)
    metrics = harness.run(dataset, name=safe, n=bn, checkpoint_every=ck_every)

    if bool(getattr(args, "bench_mtier", False)):
        row = dict(metrics)
        row["name"] = ds_name
        harness.mtier_table([row])
    elif _cli_out_json(args):
        print(json.dumps(metrics, ensure_ascii=False))
    else:
        print(
            f"harness={safe}\taccuracy={metrics.get('accuracy')}\tauroc={metrics.get('auroc')}\t"
            f"abstention_rate={metrics.get('abstention_rate')}\tsmece={metrics.get('smece')}\t"
            f"n={metrics.get('n')}\tstatus={metrics.get('status')}",
        )
    return 0


def _cmd_bench(args: argparse.Namespace) -> int:
    resume = str(getattr(args, "bench_resume", "") or "").strip()
    if resume:
        return _bench_resume(args)
    if bool(getattr(args, "bench_multi_model", False)):
        return _bench_multi_model(args)
    if bool(getattr(args, "bench_sigma_harness", False)):
        return _bench_sigma_harness(args)
    from cos import SigmaGate
    from cos.bench import DATASET_NAMES, SigmaBench

    if bool(getattr(args, "bench_mtier", False)):
        from cos.eval.mtier import mtier_payload, print_mtier

        snap = SigmaBench().mtier_v2()
        if _cli_out_json(args):
            bundle = {
                "not_agi_achieved": True,
                "benchmark_strategy": mtier_payload(),
                "harness_mtier_v2": snap,
            }
            print(json.dumps(bundle, ensure_ascii=False, indent=2))
            return 0
        print_mtier()
        print("\n--- Harness rows (md) ---\n")
        print(_fmt_mtier_text(snap))
        return 0

    ds = str(getattr(args, "bench_dataset", "") or "").strip()
    if not ds:
        print(
            "cos bench: pass --dataset NAME (toy lab), --sigma-harness, --mtier, --multi-model, "
            "or --resume CHECKPOINT.jsonl. Full harnesses live in a git checkout (benchmarks/, Makefile). "
            "Extras: pip install 'creation-os[probes,dev]'.",
            file=sys.stderr,
        )
        return 1

    if ds not in DATASET_NAMES:
        print(f"cos bench: unknown dataset {ds!r} (known: {', '.join(DATASET_NAMES)})", file=sys.stderr)
        return 1

    def _toy_model(prompt: str, ref: str = "") -> str:
        del ref
        pl = str(prompt).lower().replace(" ", "")
        if "2+2" in pl:
            return "4"
        if "france" in str(prompt).lower():
            return "Paris"
        return "unsure"

    gate = SigmaGate()
    bench = SigmaBench()
    behavioral = bool(getattr(args, "bench_behavioral", False))
    out = bench.run(ds, gate, _toy_model, behavioral=behavioral)
    if _cli_verbose(args):
        out = dict(dict(out), note="toy ΣBench rows; not a downloaded MMLU/TruthfulQA harness")
    if _cli_out_json(args):
        print(json.dumps(out, ensure_ascii=False))
        return 0
    print(f"dataset={out['dataset']}\tM_tier={out['M_tier']}\tAUROC={out['AUROC']}\tECE={out['ECE']}")
    print(f"accuracy={out['accuracy']}\tabstention_rate={out['abstention_rate']}\tn={out['n']}")
    if behavioral and "bins" in out:
        print(json.dumps({"behavioral_bins": out.get("bins")}, ensure_ascii=False))
    return 0


def _cmd_chat(args: argparse.Namespace) -> int:
    """OpenAI-compatible multi-turn σ-chat (local vLLM / SGLang / cloud) or ``--offline`` lab stub."""
    if bool(getattr(args, "chat_offline", False)):
        from cos.pipeline import Pipeline

        prompt = str(getattr(args, "chat_prompt", "") or "").strip()
        if not prompt:
            print("cos chat: --offline requires --prompt", file=sys.stderr)
            return 1
        model_label = str(getattr(args, "chat_model", "stub") or "stub")

        class _CliEchoModel:
            def generate(self, p: str, **_kw: Any) -> str:
                return f"[{model_label}] {p[:500]}"

        pipe = Pipeline(model=_CliEchoModel())
        res = pipe.run(prompt)
        payload = {
            "text": res.text,
            "sigma": res.sigma,
            "verdict": res.verdict,
            "model": model_label,
            "network_calls": 0,
            "mode": "offline_chat_lab",
        }
        print(json.dumps(payload, ensure_ascii=False))
        return 0

    from cos.chat import run_from_cli_args

    return run_from_cli_args(args)


def _cmd_explain(args: argparse.Namespace) -> int:
    from cos import SigmaGate

    gate = SigmaGate()
    prompt = str(getattr(args, "explain_prompt", "") or "")
    response = str(getattr(args, "explain_response", "") or "")
    sigma, verdict = gate.score(prompt, response)
    reasons: List[str] = []
    if sigma < gate.threshold_accept:
        reasons.append("sigma_below_threshold_accept")
    elif sigma < gate.threshold_abstain:
        reasons.append("between_threshold_accept_and_threshold_abstain_rethink_band")
    else:
        reasons.append("sigma_at_or_above_threshold_abstain")
    body = {
        "sigma": sigma,
        "verdict": verdict,
        "threshold_accept": gate.threshold_accept,
        "threshold_abstain": gate.threshold_abstain,
        "reasons": reasons,
        "note": "lite gate uses entropy on response; LSD/probe may differ.",
    }
    if _cli_out_json(args):
        print(json.dumps(body, ensure_ascii=False))
        return 2 if verdict == "ABSTAIN" else 0
    print(json.dumps(body, ensure_ascii=False, indent=2))
    return 2 if verdict == "ABSTAIN" else 0


def _cmd_cascade_cli(args: argparse.Namespace) -> int:
    from cos import SigmaGate

    gate = SigmaGate()
    prompt = str(getattr(args, "cascade_prompt", "") or "")
    response = str(getattr(args, "cascade_response", "") or "")
    out = gate.score_cascade(prompt, response)
    if _cli_verbose(args):
        out = dict(out, gate_mode=gate._mode)
    if _cli_out_json(args):
        print(json.dumps(out, ensure_ascii=False))
        return 2 if out.get("verdict") == "ABSTAIN" else 0
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return 2 if out.get("verdict") == "ABSTAIN" else 0


def _cmd_health(args: argparse.Namespace) -> int:
    import platform

    try:
        from cos import __version__ as cos_ver
    except ImportError:
        cos_ver = "unknown"
    deps: Dict[str, str] = {}
    for mod in ("fastapi", "uvicorn", "langchain_core", "openai"):
        try:
            __import__(mod if mod != "langchain_core" else "langchain_core")
            deps[mod] = "import_ok"
        except ImportError:
            deps[mod] = "missing"
    body = {
        "ok": True,
        "python": platform.python_version(),
        "platform": platform.platform(),
        "creation_os": cos_ver,
        "optional_deps": deps,
    }
    if _cli_out_json(args):
        print(json.dumps(body, ensure_ascii=False))
        return 0
    print(json.dumps(body, ensure_ascii=False, indent=2))
    return 0


def _cmd_offline(args: argparse.Namespace) -> int:
    if not bool(getattr(args, "offline_verify", False)):
        print("cos offline: pass --verify to run air-gap heuristics (DNS + TCP probes + Fabric.boot)", file=sys.stderr)
        return 2
    from cos.offline import OfflineVerifier

    result = OfflineVerifier().verify()
    if _cli_out_json(args):
        print(json.dumps(result, ensure_ascii=False, default=str))
        return 0 if result.get("verdict") == "OFFLINE" else 1
    print(f"Status: {result.get('verdict')}")
    print(f"Air-gapped (heuristic): {result.get('air_gapped')}")
    disc = result.get("disclaimer") or ""
    if disc:
        print(f"Note: {disc}")
    return 0 if result.get("verdict") == "OFFLINE" else 1


def _cmd_hardware(args: argparse.Namespace) -> int:
    import json

    from cos.hardware import HardwareInfo

    info = HardwareInfo().detect()
    if _cli_out_json(args):
        print(json.dumps(info, ensure_ascii=False))
        return 0
    print(f"Platform: {info['platform']} {info['machine']}")
    print(f"RAM: {info['ram_gb']} GB")
    gpu = info.get("gpu") or {}
    print(f"GPU: {gpu.get('type')} — {gpu.get('info', '')}")
    rec = info.get("recommendation") or {}
    print("")
    print(f"Recommended model tier: {rec.get('model', '')}")
    print(f"Backend: {rec.get('backend', '')}")
    print(f"Expected speed: {rec.get('expected_speed', '')}")
    return 0


def _cmd_layers(args: argparse.Namespace) -> int:
    import json

    from cos.fabric import Fabric

    f = Fabric()
    layers = f.layer_status()
    if _cli_out_json(args):
        print(json.dumps(layers, ensure_ascii=False))
        return 0
    for layer, info in layers.items():
        cov_pct = int(round(float(info["coverage"]) * 100))
        filled = max(0, min(10, cov_pct // 10))
        bar = "\u2588" * filled + "\u2591" * (10 - filled)
        n_lo = len(info["loaded"])
        n_mi = len(info["missing"])
        print(f"  {layer:15s} {bar} {cov_pct:3d}% ({n_lo}/{n_lo + n_mi})")
        if info["missing"]:
            print(f"    missing: {', '.join(info['missing'])}")
    return 0


def _cmd_registry_cli(args: argparse.Namespace) -> int:
    from pathlib import Path

    from cos.sigma_mcp_registry import SigmaMCPRegistry

    path = Path(str(getattr(args, "registry_path", "") or "~/.cos/sigma_mcp_registry.json")).expanduser()
    if not bool(getattr(args, "registry_list", False)):
        print("cos registry: pass --list", file=sys.stderr)
        return 1
    reg = SigmaMCPRegistry.load(path)
    rows = [{"id": sid, **data} for sid, data in sorted(reg.servers.items())]
    out = {"path": str(path), "servers": rows}
    if _cli_out_json(args):
        print(json.dumps(out, ensure_ascii=False))
        return 0
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return 0


def _cmd_cost_cli(args: argparse.Namespace) -> int:
    act = getattr(args, "cost_action", None)
    if act in ("summary", "budget", "reset"):
        from cos.cost import CostManager

        bm = float(getattr(args, "cost_manager_budget", 10.0) or 10.0)
        cm = CostManager(budget=bm)
        if act == "summary":
            out = cm.summary()
            if _cli_out_json(args):
                print(json.dumps(out, default=str, ensure_ascii=False))
            else:
                print(json.dumps(out, ensure_ascii=False, indent=2))
            return 0
        if act == "budget":
            if _cli_out_json(args):
                print(json.dumps({"budget": cm.budget, "remaining": cm.remaining()}, ensure_ascii=False))
            else:
                print(f"Budget: ${cm.budget:.2f}")
                print(f"Remaining: ${cm.remaining():.2f}")
            return 0
        cm.reset()
        body = {"ok": True, "budget": cm.budget, "spent": cm.spent, "history_entries": len(cm.history)}
        if _cli_out_json(args):
            print(json.dumps(body, ensure_ascii=False))
        else:
            print(json.dumps(body, ensure_ascii=False, indent=2))
        return 0

    if bool(getattr(args, "cost_route", False)):
        from cos.sigma_cost import LabCostGate, SigmaBillingMeter, SigmaCost, save_lab_state

        prompt = str(getattr(args, "cost_route_prompt", "") or "").strip()
        models_raw = str(getattr(args, "cost_models", "") or "").strip()
        st_path = Path(str(getattr(args, "cost_state_path", "") or "").strip()).expanduser()
        if not prompt or not models_raw:
            print("cos cost --route requires --prompt and --models", file=sys.stderr)
            return 2
        chain = [m.strip() for m in models_raw.split(",") if m.strip()]

        def _gen(model: str, p: str) -> str:
            if model == "bitnet-2b":
                return "4"
            return "maybe 5"

        cost = SigmaCost(LabCostGate(), generate=_gen)
        out = cost.route_by_cost(prompt, models=chain)
        st_path.parent.mkdir(parents=True, exist_ok=True)
        save_lab_state(st_path, cost, SigmaBillingMeter())
        print(json.dumps(out, ensure_ascii=False))
        return 0

    if not bool(getattr(args, "cost_report", False)):
        print("cos cost: pass summary|budget|reset (and optional --budget USD), or --report or --route", file=sys.stderr)
        return 1
    from cos.bench import SigmaBench

    ds = str(getattr(args, "cost_dataset", "lab") or "lab")
    rep = SigmaBench().cost_report(ds, units=float(getattr(args, "cost_units", 1.0) or 1.0))
    if _cli_out_json(args):
        print(json.dumps(rep, ensure_ascii=False))
        return 0
    print(json.dumps(rep, ensure_ascii=False, indent=2))
    return 0


def _cmd_world_cli(args: argparse.Namespace) -> int:
    from cos.jepa import SigmaJEPA

    wm = SigmaJEPA(
        dim=int(getattr(args, "world_dim", 256) or 256),
    )
    obs_seq = [str(x) for x in (getattr(args, "world_observe", None) or [])]
    if not obs_seq:
        print("cos world: pass --observe TEXT [TEXT ...]", file=sys.stderr)
        return 1
    rows: List[Dict[str, Any]] = []
    for obs in obs_seq:
        result = wm.step(obs)
        rows.append(dict(result))
        if not _cli_out_json(args):
            print(
                f"  [{result['verdict']}] σ={float(result['σ']):.3f} surprise={result['surprise']}",
            )
    payload: Dict[str, Any] = {
        "avg_sigma": wm.avg_σ(),
        "surprise_rate": wm.surprise_rate(),
        "history": rows,
    }
    if _cli_out_json(args):
        print(json.dumps(payload, ensure_ascii=False))
        return 0
    print(f"\nAvg σ: {wm.avg_σ():.3f}, Surprise rate: {wm.surprise_rate():.1%}")
    return 0


def _cmd_causal_cli(args: argparse.Namespace) -> int:
    import json

    from cos.causal import CausalGraph
    from cos.graph_export import load_graph_from_json

    loadp = str(getattr(args, "causal_load_json", "") or "").strip()
    cg: CausalGraph
    if loadp:
        sg = load_graph_from_json(loadp)
        cg = sg.to_causal_graph()
    else:
        cg = CausalGraph()

    action = str(getattr(args, "causal_action", "") or "").strip()
    var = str(getattr(args, "causal_var", "") or "").strip()
    target = str(getattr(args, "causal_target", "") or "").strip()

    if action in ("causes", "effects", "do", "why") and not var:
        print("cos causal: --var is required for this action", file=sys.stderr)
        return 2

    if action == "causes":
        payload = cg.causes_of(var)
    elif action == "effects":
        payload = cg.effects_of(var)
    elif action == "do":
        payload = cg.do(var)
    elif action == "why":
        payload = cg.root_cause(var)
    elif action == "what-if":
        try:
            observed = json.loads(str(getattr(args, "causal_observed", "{}") or "{}"))
            intervention = json.loads(str(getattr(args, "causal_intervene", "{}") or "{}"))
        except json.JSONDecodeError as exc:
            print(f"cos causal: invalid JSON: {exc}", file=sys.stderr)
            return 2
        outv = target or var
        if not outv:
            print("cos causal: what-if needs --target (outcome variable) or --var", file=sys.stderr)
            return 2
        payload = cg.counterfactual(observed, intervention, outv)
    else:
        print("cos causal: unknown action", file=sys.stderr)
        return 2

    if _cli_out_json(args):
        print(json.dumps(payload, default=str, ensure_ascii=False))
    else:
        print(json.dumps(payload, default=str, ensure_ascii=False, indent=2))
    return 0


def _cmd_graph_cli(args: argparse.Namespace) -> int:
    from cos.graph import SigmaGraph
    from cos.graph_export import GraphExport, load_graph_from_json

    loadp = str(getattr(args, "graph_load_json", "") or "").strip()
    g = load_graph_from_json(loadp) if loadp else SigmaGraph()

    hop_from = str(getattr(args, "graph_hop_from", "") or "").strip()
    hop_to = str(getattr(args, "graph_hop_to", "") or "").strip()
    exp = getattr(args, "graph_export", None)
    wants_export = bool(exp) and len(exp) == 2
    viz_path = str(getattr(args, "graph_viz", "") or "").strip()

    add_triple = getattr(args, "graph_add", None)
    did_add = False
    added_payload: Optional[Dict[str, Any]] = None
    if add_triple and len(add_triple) == 3:
        s, r, o = (str(x) for x in add_triple)
        raw_sig = getattr(args, "graph_sigma", None)
        if raw_sig is not None and str(raw_sig).strip() != "":
            added_payload = g.add(s, r, o, sigma=float(raw_sig))
        else:
            added_payload = g.add(s, r, o)
        did_add = True

    if hop_from and hop_to:
        path = g.multi_hop(hop_from, hop_to, max_hops=int(getattr(args, "graph_hops", 3) or 3))
        if _cli_out_json(args):
            print(json.dumps(path, default=str, ensure_ascii=False))
        else:
            print(json.dumps(path, default=str, ensure_ascii=False, indent=2))
        return 0

    if wants_export:
        mode, outp = str(exp[0]).lower().strip(), str(exp[1]).strip()  # type: ignore[index]
        if mode == "json":
            GraphExport(g).to_json(outp)
        elif mode == "obsidian":
            GraphExport(g).to_obsidian(outp)
        else:
            print(f"cos graph: unknown export mode {mode!r} (use json|obsidian)", file=sys.stderr)
            return 2

    if viz_path:
        try:
            from cos.graph_viz import visualize
        except ImportError as exc:
            print(f"cos graph: {exc}", file=sys.stderr)
            return 1
        visualize(g, viz_path)

    if did_add and not wants_export and not viz_path:
        out_add = {"added": added_payload}
        if _cli_out_json(args):
            print(json.dumps(out_add, ensure_ascii=False))
        else:
            print(json.dumps(out_add, ensure_ascii=False, indent=2))
        return 0

    if wants_export or viz_path or loadp:
        summary = {"stats": g.stats()}
        if _cli_out_json(args):
            print(json.dumps(summary, ensure_ascii=False))
        else:
            print(json.dumps(summary, ensure_ascii=False, indent=2))
        return 0

    print(
        "cos graph: pass --add S R O, --hop-from/--hop-to, --load-json, --export MODE PATH, or --viz PATH",
        file=sys.stderr,
    )
    return 1


def _cmd_dream_cli(args: argparse.Namespace) -> int:
    from cos.dream import run_dream_maintenance
    from cos.graph import SigmaGraph
    from cos.graph_export import GraphExport, load_graph_from_json

    loadp = str(getattr(args, "dream_load_json", "") or "").strip()
    g = load_graph_from_json(loadp) if loadp else SigmaGraph()
    add_t = getattr(args, "dream_add", None)
    if add_t and len(add_t) == 3:
        g.add(str(add_t[0]), str(add_t[1]), str(add_t[2]))

    mem = None
    if bool(getattr(args, "dream_with_memory", False)):
        from cos.memory import SigmaMemory

        mem = SigmaMemory(gate=g.gate, graph=g)

    out = run_dream_maintenance(
        g,
        memory=mem,
        dedup_threshold=float(getattr(args, "dream_dedup_threshold", 0.93) or 0.93),
        max_age_days=float(getattr(args, "dream_max_age_days", 30.0) or 30.0),
        run_decay=not bool(getattr(args, "dream_no_decay", False)),
        run_dedup=not bool(getattr(args, "dream_no_dedup", False)),
        run_orphans=not bool(getattr(args, "dream_no_orphans", False)),
        run_infer=not bool(getattr(args, "dream_no_infer", False)),
    )
    savep = str(getattr(args, "dream_save_json", "") or "").strip()
    if savep:
        GraphExport(g).to_json(savep)
        out = {**out, "saved_json": savep}
    if _cli_out_json(args):
        print(json.dumps(out, default=str, ensure_ascii=False))
    else:
        print(json.dumps(out, default=str, ensure_ascii=False, indent=2))
    return 0


def _cmd_ingest_cli(args: argparse.Namespace) -> int:
    from cos.graph import SigmaGraph
    from cos.graph_export import GraphExport, load_graph_from_json
    from cos.ingest import SigmaIngest

    path = str(getattr(args, "ingest_path", "") or "").strip()
    if not path:
        print("cos ingest: pass a FILE path", file=sys.stderr)
        return 2
    try:
        loadp = str(getattr(args, "ingest_load_json", "") or "").strip()
        g = load_graph_from_json(loadp) if loadp else SigmaGraph()
        report = SigmaIngest(g).ingest(path)
        out = {**report, "graph_stats": g.stats(), "triples_added": report.get("accepted", 0)}
        savep = str(getattr(args, "ingest_save_json", "") or "").strip()
        if savep:
            GraphExport(g).to_json(savep)
            out = {**out, "saved_json": savep}
    except (OSError, ValueError) as exc:
        print(f"cos ingest: {exc}", file=sys.stderr)
        return 2
    if _cli_out_json(args):
        print(json.dumps(out, default=str, ensure_ascii=False))
    else:
        print(json.dumps(out, default=str, ensure_ascii=False, indent=2))
    return 0


def _cmd_rag(args: argparse.Namespace) -> int:
    from cos.rag import SigmaRAG

    action = str(getattr(args, "rag_action", "") or "").strip()
    store = str(getattr(args, "rag_store_dir", "") or "").strip()
    rag = SigmaRAG(store_dir=store) if store else SigmaRAG()
    if action == "ingest":
        fp = str(getattr(args, "rag_file", "") or "").strip()
        if not fp:
            print("cos rag ingest: --file PATH required", file=sys.stderr)
            return 2
        try:
            text = Path(fp).read_text(encoding="utf-8")
        except OSError as exc:
            print(f"cos rag ingest: {exc}", file=sys.stderr)
            return 2
        result = rag.ingest(text, source=fp)
        print(f"Ingested: {result['chunks_added']} chunks")
        return 0
    if action == "query":
        q = str(getattr(args, "rag_query", "") or "").strip()
        if not q:
            print("cos rag query: --query TEXT required", file=sys.stderr)
            return 2
        result = rag.query(q)
        sig = float(result.get("σ", 0.0))
        verdict = str(result.get("verdict", ""))
        print(f"[σ={sig:.3f} {verdict}]")
        print(f"Context from {int(result.get('context_chunks', 0))} chunks")
        return 0
    if action == "stats":
        print(json.dumps(rag.stats(), indent=2, ensure_ascii=False))
        return 0
    print("cos rag: unknown action", file=sys.stderr)
    return 2


def _cmd_voice_cli(args: argparse.Namespace) -> int:
    from cos.voice import SigmaVoice, listen, sigma_before_speak, speak

    act = getattr(args, "voice_action", None)
    mock = str(getattr(args, "voice_mock", "") or "").strip()
    audio = str(getattr(args, "voice_audio", "") or "").strip()
    speak_txt = str(getattr(args, "voice_speak", "") or "").strip()
    check_txt = str(getattr(args, "voice_check", "") or "").strip()
    vin = str(getattr(args, "voice_input", "") or "").strip()
    vtxt = str(getattr(args, "voice_text", "") or "").strip()
    whisper_model = str(getattr(args, "voice_whisper_model", "tiny") or "tiny").strip()
    kokoro_voice = str(getattr(args, "voice_kokoro_voice", "af_bella") or "af_bella").strip()
    out: Dict[str, Any]

    if act == "status":
        v = SigmaVoice(whisper_model=whisper_model, kokoro_voice=kokoro_voice)
        v.boot()
        out = dict(v.available())
        out.update(v.status())
        if _cli_out_json(args):
            print(json.dumps(out, ensure_ascii=False))
        else:
            print(json.dumps(out, indent=2, ensure_ascii=False))
        return 0

    if act == "listen":
        act = "transcribe"

    if act == "transcribe":
        p = vin or audio
        if not p:
            print("cos voice transcribe: pass --input PATH", file=sys.stderr)
            return 2
        v = SigmaVoice(whisper_model=whisper_model, kokoro_voice=kokoro_voice)
        result = v.transcribe(p)
        if _cli_out_json(args):
            print(json.dumps(result, default=str, ensure_ascii=False))
        else:
            sig = result.get("σ", result.get("sigma", 0.0))
            print(f"[σ={float(sig):.3f}] {result.get('text', '')}")
        return 0

    if act == "speak":
        t = vtxt or speak_txt
        if not t:
            print("cos voice speak: pass --text STR", file=sys.stderr)
            return 2
        v = SigmaVoice(whisper_model=whisper_model, kokoro_voice=kokoro_voice)
        result = v.synthesize(t)
        if _cli_out_json(args):
            print(json.dumps(result, default=str, ensure_ascii=False))
        else:
            sig = result.get("σ", result.get("sigma", 0.0))
            print(f"[σ={float(sig):.3f}] → {result.get('path', result.get('error', ''))}")
        return 0

    if act == "chat":
        p = vin or audio
        if not p:
            print("cos voice chat: pass --input PATH (audio file)", file=sys.stderr)
            return 2
        v = SigmaVoice(whisper_model=whisper_model, kokoro_voice=kokoro_voice)
        result = v.process_voice(p)
        if _cli_out_json(args):
            print(json.dumps(result, default=str, ensure_ascii=False))
        else:
            print(json.dumps(result, default=str, ensure_ascii=False, indent=2))
        return 0

    if mock:
        out = listen(mock_text=mock)
    elif audio:
        try:
            voice = SigmaVoice(whisper_model=whisper_model, kokoro_voice=kokoro_voice)
            raw = voice.listen(audio_path=audio)
            out = {
                "text": raw.get("text", ""),
                "sigma_transcription": raw.get("sigma_transcription", raw.get("sigma")),
                "verdict": raw.get("verdict"),
                "sigma": raw.get("sigma"),
            }
        except ImportError as exc:
            print(f"cos voice: {exc}", file=sys.stderr)
            return 1
    elif speak_txt:
        captured: list[str] = []
        out = speak(speak_txt, play_fn=lambda s: captured.append(s))
        out["captured"] = captured
    elif check_txt:
        out = sigma_before_speak(check_txt)
    else:
        print(
            "cos voice: use ACTION transcribe|listen|speak|chat|status | "
            "legacy: --listen-mock TEXT | --audio PATH | --speak TEXT | --check TEXT",
            file=sys.stderr,
        )
        return 2
    if _cli_out_json(args):
        print(json.dumps(out, default=str, ensure_ascii=False))
    else:
        print(json.dumps(out, default=str, ensure_ascii=False, indent=2))
    return 0


def _cmd_ui_cli(args: argparse.Namespace) -> int:
    try:
        from cos.ui.dashboard import run_dashboard
    except ImportError as exc:
        print(f"cos ui: {exc}", file=sys.stderr)
        return 1
    run_dashboard(
        host=str(getattr(args, "ui_host", "127.0.0.1") or "127.0.0.1"),
        port=int(getattr(args, "ui_port", 8080) or 8080),
        show=not bool(getattr(args, "ui_headless", False)),
        native=bool(getattr(args, "ui_native", False)),
    )
    return 0


def _cmd_hdc_cli(args: argparse.Namespace) -> int:
    import json

    from cos.hypervector import HyperVector, SigmaHDC

    sub = str(getattr(args, "hdc_cmd", "") or "").strip()
    dim = int(getattr(args, "hdc_dim", 10_000) or 10_000)
    seed = int(getattr(args, "hdc_seed", 0) or 0)
    lab = SigmaHDC(dim=dim, seed=seed)
    if sub == "encode":
        s, r, o = str(args.hdc_s), str(args.hdc_r), str(args.hdc_o)
        hv = lab.encode_triple(s, r, o)
        payload = {
            "subject": s,
            "relation": r,
            "object": o,
            "dim": hv.dim,
            "memory_bytes": hv.memory_bytes(),
            "bitpacked_bytes": hv.bitpacked_bytes(),
        }
        print(json.dumps(payload, ensure_ascii=False))
        return 0
    if sub == "query":
        s, r, o = str(args.hdc_s), str(args.hdc_r), str(args.hdc_o)
        t = lab.encode_triple(s, r, o)
        q = lab.query_triple(t, s, r)
        po = HyperVector.permute(lab.book[o], 1)
        sim = HyperVector.similarity(q, po)
        print(json.dumps({"similarity": round(sim, 6), "dim": dim}, ensure_ascii=False))
        return 0
    if sub == "sequence":
        raw = str(getattr(args, "hdc_sequence", "") or "")
        tokens = [x.strip() for x in raw.split(",") if x.strip()]
        if len(tokens) < 2:
            print("cos hdc sequence: pass comma-separated --tokens", file=sys.stderr)
            return 2
        s_sig = lab.sequence_sigma(tokens)
        print(json.dumps({"tokens": tokens, "sequence_sigma": round(s_sig, 6)}, ensure_ascii=False))
        return 0
    print("cos hdc: use encode | query | sequence", file=sys.stderr)
    return 2


def _cmd_evolve_step(args: argparse.Namespace) -> int:
    et = str(getattr(args, "evolve_target", "") or "").strip()
    eg = str(getattr(args, "evolve_goal", "") or "").strip()
    rsi_n = int(getattr(args, "evolve_rsi_steps", 0) or 0)
    if rsi_n > 0:
        from cos.evolve import SigmaEvolve
        from cos.sigma_gate import SigmaGate

        ev = SigmaEvolve()
        gate = SigmaGate()
        eval_data = [
            ("omega rsi lab question text", "omega rsi lab answer text repeated", True),
        ]
        hist = ev.run(gate, eval_data, max_steps=max(1, rsi_n), formal=None)
        payload: Dict[str, Any] = {"mode": "rsi", "steps_requested": rsi_n, "history": hist}
        if _cli_out_json(args):
            print(json.dumps(payload, ensure_ascii=False))
            return 0
        print(json.dumps(payload, ensure_ascii=False, indent=2))
        return 0
    if et and eg and not bool(getattr(args, "evolve_step", False)):
        from cos.sigma_evolve import SigmaEvolve as SigmaEvolveV137
        from cos.sigma_evolve import ToyEvolveEvaluator
        from cos.sigma_split import SigmaSplitGate

        class _MiniEvolveModel:
            def generate(self, prompt: str) -> str:
                _ = prompt
                return "--- a/x.py\n+++ b/x.py\n@@\n+ok\n"

        evo = SigmaEvolveV137(SigmaSplitGate(), _MiniEvolveModel(), ToyEvolveEvaluator(0.0))
        card = evo.propose_improvement(et, eg)
        print(json.dumps(card, ensure_ascii=False, default=str))
        return 0
    if not bool(getattr(args, "evolve_step", False)):
        print("cos evolve: pass --step or --target MODULE --goal TEXT", file=sys.stderr)
        return 1
    from cos.evolve import SigmaEvolve

    ev = SigmaEvolve()
    system: Dict[str, Any] = {"step": 0, "note": "cli_lab"}

    def _eval_fn(_sys: Dict[str, Any]) -> float:
        return 0.5

    out = ev.improve_loop(system, _eval_fn, max_iters=int(getattr(args, "evolve_iters", 1) or 1))
    if _cli_out_json(args):
        print(json.dumps(out, ensure_ascii=False))
        return 0
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return 0


def _cmd_redteam_cli(args: argparse.Namespace) -> int:
    if bool(getattr(args, "redteam_gate_campaign", False)):
        from cos import SigmaGate
        from cos.redteam import SigmaRedTeam

        n = int(getattr(args, "redteam_per_case", 5) or 5)
        rt = SigmaRedTeam(SigmaGate())
        test_cases = [
            ("Capital of France?", "Paris", "Berlin"),
            ("Who painted Mona Lisa?", "Leonardo da Vinci", "Picasso"),
            ("What is 2+2?", "4", "5"),
            ("Largest planet?", "Jupiter", "Mars"),
        ]
        report = rt.run(test_cases, n_attacks_per_case=n)
        if _cli_out_json(args):
            print(json.dumps(report, ensure_ascii=False, default=str))
            return 0
        print(f"Attacks: {report['total_attacks']}")
        print(f"False accepts: {report['false_accepts']}")
        print(f"Rate: {report['false_accept_rate']:.1%}")
        print(f"Severity: {report['severity']}")
        if report["worst_cases"]:
            print("\nWorst cases:")
            for wc in report["worst_cases"][:3]:
                print(f"  [{wc['attack_type']}] σ={wc['σ_adversarial']:.3f}")
        return 0

    target = str(getattr(args, "redteam_target", "mock") or "mock")
    if target != "mock":
        print("cos redteam: only --target mock is wired in minimal install", file=sys.stderr)
        return 1
    from cos import SigmaGate
    from cos.sigma_red_team import MockRedTeamModel, SigmaRedTeam

    n = int(getattr(args, "redteam_attacks", 10) or 10)
    gate = SigmaGate()
    rt = SigmaRedTeam(gate, MockRedTeamModel())
    results = rt.run_all([], n_per_attack=max(1, n))
    if _cli_out_json(args):
        print(json.dumps(results, ensure_ascii=False))
        return 0
    print(json.dumps(results, ensure_ascii=False, indent=2))
    return 0


def _cmd_pipe(args: argparse.Namespace) -> int:
    from cos import Pipeline

    prompt = str(getattr(args, "pipe_prompt", "") or "").strip()
    resp = str(getattr(args, "pipe_response", "") or "").strip()
    pipe = Pipeline()
    if resp:
        result = pipe.run(prompt, response=resp)
    else:
        result = pipe.run(prompt)
    print(f"σ={result.sigma:.4f} {result.verdict}")
    if result.text:
        print(result.text)
    if result.reason and result.verdict in ("BLOCKED", "NO_MODEL", "OUTPUT_BLOCKED"):
        print(result.reason, file=sys.stderr)
    return 0


def _cmd_stats(args: argparse.Namespace) -> int:
    from cos import Pipeline

    _ = args
    print(json.dumps(Pipeline().stats.to_dict(), indent=2, ensure_ascii=False))
    return 0


def _cmd_stream(args: argparse.Namespace) -> int:
    from cos.stream import SigmaStream

    prompt = str(getattr(args, "stream_prompt", "") or "").strip()
    thresh = float(getattr(args, "interrupt_threshold", 0.8) or 0.8)
    tok_raw = str(getattr(args, "stream_tokens", "") or "").strip()
    if tok_raw:
        tokens = tok_raw.split()
    else:
        tokens = ["The", "answer", "is", "42"]
    stream = SigmaStream(interrupt_threshold=thresh)
    for event in stream.score_stream(prompt, tokens):
        print(event)
    return 0


def _cmd_snapshot(args: argparse.Namespace) -> int:
    from cos import Pipeline
    from cos.snapshot import SnapshotManager

    action = str(getattr(args, "snapshot_action", "") or "")
    pipe = Pipeline()
    mgr = SnapshotManager(pipe)
    label = getattr(args, "snapshot_label", None)

    if action == "save":
        print(json.dumps(mgr.checkpoint(label), ensure_ascii=False))
    elif action == "rollback":
        print(json.dumps(mgr.rollback(label), ensure_ascii=False))
    elif action == "list":
        for s in mgr.list_snapshots():
            lbl = s.get("label") or ""
            print(f"  [{s['index']}] {lbl} ({s['checksum']})")
    elif action == "diff":
        print(json.dumps(mgr.diff(), ensure_ascii=False))
    else:
        print("cos snapshot: unknown action", file=sys.stderr)
        return 2
    return 0


def _cmd_calibrate(args: argparse.Namespace) -> int:
    from cos.calibrate import CalibrationReport, SigmaCalibrator, load_calibration_pairs

    action = str(getattr(args, "calibrate_action", "") or "")
    data_path_str = str(getattr(args, "calibrate_data", "") or "").strip()
    method = str(getattr(args, "calibrate_method", "platt") or "platt")
    out_path = Path(str(getattr(args, "calibrate_out", "calibration.json") or "calibration.json"))
    model_path_str = str(getattr(args, "calibrate_model", "") or "").strip()

    if action == "lab":
        ds = str(getattr(args, "calibrate_dataset", "") or "").strip()
        if not ds:
            print("cos calibrate lab: pass --dataset NAME", file=sys.stderr)
            return 1
        from cos import SigmaGate
        from cos.bench import DATASET_NAMES, SigmaBench

        if ds not in DATASET_NAMES:
            print(
                f"cos calibrate lab: unknown dataset {ds!r} (known: {', '.join(DATASET_NAMES)})",
                file=sys.stderr,
            )
            return 1

        def _toy_model(prompt: str, ref: str = "") -> str:
            del ref
            pl = str(prompt).lower().replace(" ", "")
            if "2+2" in pl:
                return "4"
            if "france" in str(prompt).lower():
                return "Paris"
            return "unsure"

        gate = SigmaGate()
        bench = SigmaBench()
        out = bench.run(ds, gate, _toy_model, behavioral=True)
        if _cli_out_json(args):
            print(json.dumps(out, ensure_ascii=False))
            return 0
        print(json.dumps(out, ensure_ascii=False, indent=2))
        return 0

    if action == "fit":
        if not data_path_str:
            print("cos calibrate fit: pass --data PATH.json", file=sys.stderr)
            return 2
        pairs = load_calibration_pairs(Path(data_path_str))
        cal = SigmaCalibrator(method=method)
        cal.fit(pairs)
        cal.save(out_path)
        print(
            json.dumps(
                {"fitted": cal.fitted, "method": cal.method, "n_samples": len(pairs), "out": str(out_path)},
                ensure_ascii=False,
            )
        )
        return 0

    if action == "ece":
        if not data_path_str or not model_path_str:
            print("cos calibrate ece: pass --data and --model", file=sys.stderr)
            return 2
        pairs = load_calibration_pairs(Path(data_path_str))
        cal = SigmaCalibrator()
        cal.load(Path(model_path_str))
        ece_val = cal.ece(pairs, n_bins=int(getattr(args, "calibrate_bins", 10) or 10))
        print(json.dumps({"ece": ece_val}, ensure_ascii=False))
        return 0

    if action == "report":
        if not data_path_str or not model_path_str:
            print("cos calibrate report: pass --data and --model", file=sys.stderr)
            return 2
        pairs = load_calibration_pairs(Path(data_path_str))
        cal = SigmaCalibrator()
        cal.load(Path(model_path_str))
        rep = CalibrationReport(cal, pairs, n_bins=int(getattr(args, "calibrate_bins", 10) or 10))
        print(json.dumps(rep.generate(), ensure_ascii=False))
        return 0

    print("cos calibrate: unknown action", file=sys.stderr)
    return 2


def _cmd_serve(args: argparse.Namespace) -> int:
    try:
        from cos.serve import run
    except ImportError:
        print("cos serve: pip install 'creation-os[serve]'", file=sys.stderr)
        return 2
    host = str(getattr(args, "serve_host", "0.0.0.0") or "0.0.0.0")
    port = int(getattr(args, "serve_port", 8000) or 8000)
    run(host=host, port=port)
    return 0


def _cmd_fabric(args: argparse.Namespace) -> int:
    from cos.fabric import SigmaFabric

    fabric = SigmaFabric()
    fabric.boot()
    cmd = str(getattr(args, "fabric_cmd", "") or "")
    if cmd == "status":
        print(json.dumps(fabric.status(), indent=2, ensure_ascii=False))
        return 0
    if cmd == "process":
        prompt = str(getattr(args, "fabric_prompt", "") or "")
        if not prompt.strip():
            print("cos fabric process: --prompt is required", file=sys.stderr)
            return 2
        resp = str(getattr(args, "fabric_response", "") or "").strip()
        out = fabric.process(prompt, response=resp if resp else None)
        print(repr(out))
        print(repr(out.trace))
        return 0
    print("cos fabric: unknown subcommand", file=sys.stderr)
    return 2


def _cmd_cognitive_boot(args: argparse.Namespace) -> int:
    from cos.boot import Boot

    result = Boot().run()
    if _cli_out_json(args):
        print(json.dumps(result, ensure_ascii=False, default=str))
        return 0
    print(result["message"])
    for name, step in result["steps"].items():
        ok = step.get("status") == "OK"
        icon = "✓" if ok else "·"
        print(f"  {icon} {name}: {step.get('status', '?')}")
    return 0


def _cmd_cognitive_status(_args: argparse.Namespace) -> int:
    from cos.fabric import Fabric

    f = Fabric()
    f.boot()
    print(json.dumps(f.cognitive_state(), indent=2, ensure_ascii=False, default=str))
    return 0


def _cmd_cognitive_process(args: argparse.Namespace) -> int:
    from cos.fabric import Fabric

    f = Fabric()
    f.boot()
    result = f.process(str(getattr(args, "cognitive_input", "") or ""))
    print(json.dumps(result, indent=2, ensure_ascii=False, default=str))
    return 0


def _cmd_genesis(args: argparse.Namespace) -> int:
    from cos.eval.genesis import GenesisCheck

    path = str(getattr(args, "genesis_engram", "") or "").strip()
    g = GenesisCheck(engram_path=path if path else None)
    result = g.run()
    print(f"Genesis: {result['passed']}/{result['total']} stages passed")
    for name, r in result["stages"].items():
        mark = "ok" if r.get("passed") else "fail"
        print(f"  [{mark}] {name}")
    print(f"\n{result['note']}")
    return 0


def _cmd_repro(args: argparse.Namespace) -> int:
    from cos.eval.repro_bundle import ReproBundle

    name = str(getattr(args, "repro_name", "demo") or "demo").strip() or "demo"
    out = str(getattr(args, "repro_output", "") or "").strip()
    b = ReproBundle(name, output_dir=out if out else None)
    b.claim("Template reproducibility bundle (replace with harness-bound claims)", 3)
    b.result("template", "score", 0.75, 10, model_id="lab", config={"note": "replace with archived harness metric"})
    b.negative(
        "template_negative",
        "score",
        0.52,
        "Mandatory negative row: example failing distribution (see HaluEval in CLAIM_DISCIPLINE).",
    )
    b.limitation("Not a published harness artifact until JSON + SHA + host metadata are archived.")
    b.falsifier("Held-out evaluation contradicts the stated positive row at same metric → bundle invalid.")

    v = b.validate()
    if getattr(args, "repro_json", False):
        if not v["valid"]:
            print(json.dumps({"valid": False, "errors": v["errors"]}, indent=2), file=sys.stderr)
            return 1
        out_path = b.save()
        print(json.dumps({**v, "saved": out_path}, indent=2))
        return 0
    if not v["valid"]:
        print("repro validate failed:", "; ".join(v["errors"]), file=sys.stderr)
        return 1
    print(b.save())
    return 0


def _cmd_memory_cli(args: argparse.Namespace) -> int:
    from cos.memory import SigmaMemory

    pd = str(getattr(args, "mem_persist_dir", "") or "").strip()
    mem = SigmaMemory(persist_dir=pd) if pd else SigmaMemory()
    action = str(getattr(args, "mem_action", "") or "")
    if action == "store":
        content = str(getattr(args, "mem_content", "") or "").strip()
        if not content:
            print("cos memory store: pass --content TEXT", file=sys.stderr)
            return 2
        ctx = str(getattr(args, "mem_context", "") or "").strip()
        mtype = str(getattr(args, "mem_type", "episodic") or "episodic")
        result = mem.store(content, context=ctx, memory_type=mtype)
        sig = float(result.get("σ", result.get("sigma", 0.0)))
        print(f"Stored: σ={sig:.3f} {result.get('verdict', '')}")
        return 0
    if action == "recall":
        q = str(getattr(args, "mem_query", "") or "").strip()
        if not q:
            print("cos memory recall: pass --query TEXT", file=sys.stderr)
            return 2
        for r in mem.recall(q):
            print(f"  σ={float(r['σ']):.3f} [{r['type']}] {str(r['content'])[:80]}")
        return 0
    if action == "stats":
        print(json.dumps(mem.stats(), indent=2, ensure_ascii=False))
        return 0
    if action == "consolidate":
        result = mem.consolidate()
        print(f"Consolidated: {result}")
        return 0
    if action == "forget":
        out = mem.forget()
        print(f"Forgot {out.get('forgotten', 0)} memories (σ ≥ 0.95)")
        return 0
    return 2


def _cmd_silicon(args: argparse.Namespace) -> int:
    from cos.sigma_silicon import benchmark_targets_json, parse_sim_test, simulate_semantic

    if bool(getattr(args, "silicon_benchmark", False)):
        print(json.dumps(benchmark_targets_json(), ensure_ascii=False))
        return 0
    if bool(getattr(args, "silicon_simulate", False)):
        spec = str(getattr(args, "silicon_test", "") or "").strip()
        op, rest = parse_sim_test(spec)
        body = simulate_semantic(op, rest)
        print(json.dumps(body, ensure_ascii=False))
        return 0 if body.get("ok") else 2
    print("cos silicon: pass --benchmark or --simulate --test …", file=sys.stderr)
    return 2


def _cmd_integrations(args: argparse.Namespace) -> int:
    if bool(getattr(args, "integrations_check", False)):
        return 0
    if bool(getattr(args, "integrations_example", False)):
        return 2
    print("cos integrations: pass --check or --example", file=sys.stderr)
    return 2


def _cmd_learn(args: argparse.Namespace) -> int:
    if not bool(getattr(args, "learn_forgetting_test", False)):
        print("cos learn: pass --forgetting-test", file=sys.stderr)
        return 2
    from cos.sigma_consolidation import SigmaConsolidation, ToyContinualGate, ToyContinualModel

    apath = Path(str(getattr(args, "learn_anchors", "") or "").strip()).expanduser()
    steps = max(1, int(getattr(args, "learn_steps", 1) or 1))
    lr = float(getattr(args, "learn_lr", 0.001) or 0.001)
    rows: List[Tuple[str, str]] = []
    with apath.open(encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            obj = json.loads(line)
            if isinstance(obj, dict):
                p = obj.get("prompt") or obj.get("q")
                a = obj.get("answer") or obj.get("response") or obj.get("a")
                if p is not None and a is not None:
                    rows.append((str(p), str(a)))
    if not rows:
        print("cos learn: anchors file has no prompt/answer rows", file=sys.stderr)
        return 2
    model = ToyContinualModel()
    gate = ToyContinualGate(model)
    cons = SigmaConsolidation(gate, model)
    cons.set_anchors(rows)
    for _ in range(steps):
        out = cons.learn_step([("unrelated stable topic", "ok")], lr=lr, tolerance=0.05)
        if not out.get("learned"):
            return 1
    chk = cons.check_anchors(tolerance=0.05)
    return 0 if not chk.get("regressed") else 1


def _cmd_jepa_cli(args: argparse.Namespace) -> int:
    from cos.sigma_jepa import LabLatentEncoder, LabLatentPredictor, SigmaJEPA

    j = SigmaJEPA(LabLatentEncoder(dim=8), LabLatentPredictor(drift=0.02), None, k_raw=0.92)
    obs = str(getattr(args, "jepa_observation", "") or "")
    act = str(getattr(args, "jepa_action", "") or "")
    if bool(getattr(args, "jepa_predict", False)):
        out = j.world_model_predict(obs, act if act else None)
        print(json.dumps(out, ensure_ascii=False, default=str))
        return 0
    print("cos jepa: pass --predict (--observation, --action)", file=sys.stderr)
    return 2


def _cmd_moe_cli(args: argparse.Namespace) -> int:
    from cos.sigma_moe import load_registry, save_registry

    name = str(getattr(args, "moe_register_name", "") or "").strip()
    model = str(getattr(args, "moe_register_model", "") or "").strip()
    reg_path = Path(str(getattr(args, "moe_registry", "") or "").strip()).expanduser()
    if name:
        if not model:
            print("cos moe: --register NAME requires --model", file=sys.stderr)
            return 2
        reg = load_registry(reg_path)
        reg[name] = model
        save_registry(reg_path, reg)
        print(json.dumps({"registered": name}, ensure_ascii=False))
        return 0
    print("cos moe: pass --register EXPERT --model ID --registry PATH", file=sys.stderr)
    return 2


def _cmd_zkp_cli(args: argparse.Namespace) -> int:
    from cos.sigma_zkp import SigmaZKP, lab_sigma_zkp_gate

    anchor = str(getattr(args, "zkp_model_anchor", "") or "creation-os-lab-probe-v1").strip()
    if bool(getattr(args, "zkp_prove", False)):
        z = SigmaZKP(lab_sigma_zkp_gate(anchor))
        pr = str(getattr(args, "zkp_prompt", "") or "")
        rs = str(getattr(args, "zkp_response", "") or "")
        proof = z.prove_verdict(pr, rs)
        print(json.dumps(proof, ensure_ascii=False))
        return 0
    if bool(getattr(args, "zkp_verify", False)):
        proof_path = Path(str(getattr(args, "zkp_proof_path", "") or "").strip()).expanduser()
        proof = json.loads(proof_path.read_text(encoding="utf-8"))
        exp_mc = str(getattr(args, "zkp_expected_commitment", "") or "").strip()
        z = SigmaZKP(
            lab_sigma_zkp_gate(anchor),
            model_commitment={"algorithm": "sha256", "hash": exp_mc},
        )
        pr = str(getattr(args, "zkp_prompt", "") or "")
        rs = str(getattr(args, "zkp_response", "") or "")
        body = z.verify_proof(proof, pr, rs)
        print(json.dumps(body, ensure_ascii=False))
        return 0 if body.get("valid") else 3
    print("cos zkp: pass --prove or --verify", file=sys.stderr)
    return 2


def _cmd_spike(args: argparse.Namespace) -> int:
    from cos.sigma_spike import (
        OmegaSpikeLab,
        SigmaSpike,
        SigmaSpikeNetwork,
        benchmark_lif_vs_dense_wallclock_lab,
        benchmark_spike_vs_continuous_mj_per_token,
        hidden_from_token,
        prompt_drive_q15,
    )

    if bool(getattr(args, "spike_omega", False)):
        lab = OmegaSpikeLab(threshold=float(getattr(args, "spike_threshold", 0.05) or 0.05))
        turns = max(1, int(getattr(args, "spike_turns", 1) or 1))
        ph = [hidden_from_token(f"p{i}", dim=8) for i in range(14)]
        agg: Dict[str, Any] = {"mode": "omega_spike", "turns": turns}
        for t in range(turns):
            step = lab.step(ph)
            agg.update({f"turn_{t}": step})
        agg["lane_events"] = step.get("lane_events", 0)
        agg["sparsity"] = float(step.get("sparsity", 0.0))
        print(json.dumps(agg, ensure_ascii=False))
        return 0

    if bool(getattr(args, "spike_energy", False)):
        n_layers = max(1, int(getattr(args, "spike_layers", 14) or 14))
        net = SigmaSpikeNetwork(n_layers)
        drive = prompt_drive_q15("", spread=1.0)
        net.forward(drive)
        rep = net.energy_report()
        out = {"mode": "energy", **rep, "benchmark": benchmark_spike_vs_continuous_mj_per_token()}
        print(json.dumps(out, ensure_ascii=False))
        return 0

    if bool(getattr(args, "spike_benchmark", False)):
        n = max(1, int(getattr(args, "spike_benchmark_n", 1000) or 1000))
        body = benchmark_lif_vs_dense_wallclock_lab(n)
        body["mode"] = "lif_wallclock"
        print(json.dumps(body, ensure_ascii=False))
        return 0

    si = getattr(args, "spike_input", None)
    if si is not None and str(si).strip() != "":
        n_layers = max(1, int(getattr(args, "spike_layers", 14) or 14))
        spread = float(getattr(args, "spike_spread", 1.0) or 1.0)
        net = SigmaSpikeNetwork(n_layers)
        d = prompt_drive_q15(str(si), spread=spread)
        o = net.forward(d)
        print(json.dumps({"mode": "lif_v154", **o}, ensure_ascii=False))
        return 0

    prompt = str(getattr(args, "spike_prompt", "") or "").strip()
    toks = [t for t in prompt.split() if t]
    th = float(getattr(args, "spike_threshold", 0.05) or 0.05)
    sp = SigmaSpike(threshold=th)
    suppressed = 0
    for w in toks:
        sp.process_token(hidden_from_token(w, dim=8))
        suppressed = int(sp.stats["suppressed"])
    print(
        json.dumps(
            {
                "mode": "token_spike",
                "tokens": len(toks),
                "suppressed": suppressed,
                "threshold": th,
            },
            ensure_ascii=False,
        )
    )
    return 0


def _cmd_tiny(args: argparse.Namespace) -> int:
    from cos.sigma_tiny import tiny_footprint_json, tiny_sensor_demo

    if bool(getattr(args, "tiny_footprint", False)):
        print(json.dumps(tiny_footprint_json(), ensure_ascii=False))
        return 0
    if bool(getattr(args, "tiny_sensor", False)):
        b = int(getattr(args, "tiny_baseline", 0) or 0)
        tol = int(getattr(args, "tiny_tolerance", 1) or 1)
        r = int(getattr(args, "tiny_reading", 0) or 0)
        print(json.dumps(tiny_sensor_demo(baseline=b, tolerance=tol, reading=r), ensure_ascii=False))
        return 0
    print("cos tiny: pass --footprint or --sensor (with baseline/tolerance/reading)", file=sys.stderr)
    return 2


def _cmd_think(args: argparse.Namespace) -> int:
    """Default: full :class:`~cos.fabric.Fabric` pipeline; ``--jepa`` keeps v123 lab JSON."""
    if bool(getattr(args, "think_jepa_lab", False)):
        from cos.sigma_jepa import LabLatentEncoder, LabLatentPredictor, SigmaJEPA

        prompt = str(getattr(args, "think_prompt", "") or "").strip()
        if not prompt:
            print("cos think --jepa: pass --prompt TEXT", file=sys.stderr)
            return 2
        horizon = max(1, int(getattr(args, "think_horizon", 5) or 5))
        planning_steps = max(1, int(getattr(args, "think_planning_steps", 100) or 100))
        j = SigmaJEPA(LabLatentEncoder(dim=8), LabLatentPredictor(drift=0.02), None, k_raw=0.92)
        if bool(getattr(args, "think_visualize", False)):
            raw = str(getattr(args, "think_actions", "") or "").strip()
            actions = [a.strip() for a in raw.split(",") if a.strip()]
            out = j.latent_trajectory_for_visualize(prompt, actions, max_steps=10)
            print(json.dumps(out, ensure_ascii=False))
            return 0
        cands = [["noop", "step"], ["probe", "halt"]]
        out = j.plan_argmin_sigma(prompt, cands, horizon=horizon, planning_steps=planning_steps)
        print(json.dumps(out, ensure_ascii=False))
        return 0

    text = str(getattr(args, "think_input", "") or "").strip()
    if not text:
        print(
            "cos think: pass INPUT (full σ-Fabric pipeline), e.g. cos think 'What causes rain?'\n"
            "       or cos think --jepa --prompt '…' for v123 plan_argmin JSON.",
            file=sys.stderr,
        )
        return 2
    from cos.fabric import Fabric

    fab = Fabric()
    fab.boot()
    result = fab.process(text)
    sigma = float(result.get("σ", result.get("sigma", 0.0)))
    smeta = float(result.get("σ_meta", 0.0))
    verdict = str(result.get("verdict", "RETHINK"))
    print(f"\n[σ={sigma:.3f} σ_meta={smeta:.3f} {verdict}]")
    print(f"Latency: {result.get('latency_ms', 0):.0f}ms, layers_active: {result.get('layers_active', 0)}")
    if bool(getattr(args, "think_trace", False)):
        for row in result.get("trace") or []:
            print(f"  {row.get('layer')}: {row}")
    body = result.get("result")
    print(f"\n{body}")
    return 0


def _cmd_split(args: argparse.Namespace) -> int:
    from cos.sigma_split import SigmaSplitGate, SigmaSplitInference, ToyLocalModel, ToyRemoteModel

    inf = SigmaSplitInference(ToyLocalModel(), remote_endpoint=ToyRemoteModel(), gate=SigmaSplitGate(), spec_sigma_threshold=0.3)
    if bool(getattr(args, "split_layer", False)):
        sp = int(getattr(args, "split_point", 8) or 8)
        pr = str(getattr(args, "split_prompt", "") or "")
        _tensor, tag = inf.layer_split(pr if pr else " ", split_point=sp)
        print(json.dumps({"layer_tag": tag}, ensure_ascii=False))
        return 0
    pr = str(getattr(args, "split_prompt", "") or "")
    priv = str(getattr(args, "split_privacy", "normal") or "normal")
    body = inf.infer(pr, privacy=priv)
    print(json.dumps(body, ensure_ascii=False, default=str))
    return 0


def _cmd_fleet(args: argparse.Namespace) -> int:
    from cos.sigma_fleet import SigmaFleet

    state = Path(str(getattr(args, "fleet_state", "") or "").strip()).expanduser()
    fl = SigmaFleet()
    if state.is_file():
        raw = json.loads(state.read_text(encoding="utf-8"))
        devs = raw.get("devices") or {}
        if isinstance(devs, dict):
            for did, row in devs.items():
                if isinstance(row, dict):
                    fl.devices[str(did)] = row

    if bool(getattr(args, "fleet_register", False)):
        did = str(getattr(args, "fleet_device", "device") or "device")
        model = str(getattr(args, "fleet_model", "unknown") or "unknown")
        fl.register(
            did,
            {
                "model": model,
                "local": True,
                "avg_sigma": 0.2,
            },
        )
        state.parent.mkdir(parents=True, exist_ok=True)
        state.write_text(json.dumps({"devices": fl.devices}, indent=2, ensure_ascii=False, default=str) + "\n", encoding="utf-8")
        print(json.dumps({"registered": did}, ensure_ascii=False))
        return 0

    if bool(getattr(args, "fleet_health", False)):
        h = fl.fleet_health()
        print(json.dumps(h, ensure_ascii=False))
        return 0

    print("cos fleet: pass --register (--device, --model, --state) or --health --state", file=sys.stderr)
    return 2


def _cmd_attention(args: argparse.Namespace) -> int:
    from cos.sigma_attention import LabTokenSigmaGate, SigmaAttention

    text = str(getattr(args, "attn_input", "") or "")
    toks = text.split()
    n = max(1, len(toks))
    w = max(1, int(getattr(args, "attn_window", 4) or 4))
    thr = float(getattr(args, "attn_threshold", 0.3) or 0.3)
    d = 8
    gate = LabTokenSigmaGate(n, low_fraction=0.25, low_sigma=0.05, high_sigma=0.8)
    q = [[float((i + k) % 5) / 5.0 for k in range(d)] for i in range(n)]
    k = [list(row) for row in q]
    v = [[x * 0.9 for x in row] for row in q]
    attn = SigmaAttention(gate, window_size=w, sigma_threshold=thr)
    attn.forward(q, k, v)
    print(json.dumps({"full_tokens": attn.stats["full"], "pruned_tokens": attn.stats["pruned"]}, ensure_ascii=False))
    return 0


def _cmd_memory(args: argparse.Namespace) -> int:
    from cos.engram_v2 import EngramV2
    from cos.sigma_gate_core import Verdict

    st_path = Path(str(getattr(args, "memory_state", "") or "").strip()).expanduser()
    eg = EngramV2.load_json(st_path) if st_path.is_file() else EngramV2()
    if getattr(args, "memory_store", None) is not None:
        content = str(args.memory_store)
        sigma = float(getattr(args, "memory_sigma", 0.05) or 0.05)
        vraw = str(getattr(args, "memory_verdict", "ACCEPT") or "ACCEPT").strip().upper()
        vd = getattr(Verdict, vraw, Verdict.ACCEPT)
        out = eg.store(content, "cli", sigma, vd)
        eg.save_json(st_path)
        print(json.dumps(out, ensure_ascii=False))
        return 0
    if getattr(args, "memory_recall", None) is not None:
        tau = float(getattr(args, "memory_tau", 0.3) or 0.3)
        hits = eg.recall(str(args.memory_recall), tau=tau, max_results=10)
        print(json.dumps({"recall": hits}, ensure_ascii=False, default=str))
        return 0
    print("cos engram: use --store or --recall with --state-file", file=sys.stderr)
    return 2


def _cmd_bitnet(args: argparse.Namespace) -> int:
    from cos.sigma_bitnet import sparsity_packed, stack_forward_sigma_gated, toy_layers_for_prompt

    if bool(getattr(args, "bitnet_sparsity", False)):
        layers = toy_layers_for_prompt("cli-sparsity", dim=8, n_layers=4)
        fracs = [sparsity_packed(pk, 64) for pk, _r, _c, _sc, _ in layers]
        avg = sum(fracs) / float(len(fracs)) if fracs else 0.0
        print(json.dumps({"avg_sparsity": round(avg, 6), "per_layer": fracs}, ensure_ascii=False))
        return 0
    prompt = str(getattr(args, "bitnet_prompt", "") or "").strip()
    if not prompt:
        print("cos bitnet: pass --sparsity or --prompt …", file=sys.stderr)
        return 2
    dim = max(2, int(getattr(args, "bitnet_dim", 8) or 8))
    n_layers = max(1, int(getattr(args, "bitnet_layers", 4) or 4))
    k_raw = float(getattr(args, "bitnet_k_raw", 0.92) or 0.92)
    layers = toy_layers_for_prompt(prompt, dim=dim, n_layers=n_layers)
    seed = sum(ord(c) for c in prompt) % 97
    x0 = [(i * 11 + seed) % 101 - 50 for i in range(dim)]
    code, _out, sigs = stack_forward_sigma_gated(layers, x0, k_raw=k_raw)
    print(
        json.dumps(
            {
                "code": int(code),
                "layers_run": len(sigs),
            },
            ensure_ascii=False,
        )
    )
    return 0


def _cmd_twin(args: argparse.Namespace) -> int:
    from cos.sigma_twin import SigmaTwin, default_sigma_twin, parse_change_map, workspace_state_path

    ws = str(getattr(args, "twin_workspace", "") or "").strip()
    if not ws:
        print("cos twin: pass --workspace DIR", file=sys.stderr)
        return 2
    root = Path(ws).expanduser()
    path = workspace_state_path(root)
    if bool(getattr(args, "twin_create", False)):
        tw = default_sigma_twin()
        tw.create_twin()
        tw.save(path)
        print(json.dumps({"ok": True, "path": str(path)}, ensure_ascii=False))
        return 0
    if bool(getattr(args, "twin_experiment", False)):
        tw = SigmaTwin.load(path)
        name = str(getattr(args, "twin_exp_name", "exp") or "exp")
        changes = parse_change_map([str(getattr(args, "twin_change", "") or "")])
        from cos.sigma_twin import DEFAULT_FIXTURE

        out = tw.experiment(name, changes, DEFAULT_FIXTURE)
        tw.save(path)
        print(json.dumps(out, ensure_ascii=False, default=str))
        return 0
    print("cos twin: pass --create or --experiment (with --name, --change)", file=sys.stderr)
    return 2


def main(argv: Optional[List[str]] = None) -> int:
    argv = argv if argv is not None else sys.argv[1:]
    ap = argparse.ArgumentParser(
        prog="cos",
        description="Creation OS cos CLI — σ-gate-first helpers and lab commands.",
        epilog=_COS_HELP_EPILOG,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = ap.add_subparsers(dest="cmd", required=True)

    ver = sub.add_parser("version", help="Print creation-os package version")
    ver.add_argument("--json", action="store_true", dest="out_json", help="machine-readable output")
    ver.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    ver.set_defaults(func=_cmd_cos_version)

    idp = sub.add_parser(
        "identity",
        help="Persistent identity + narrative continuity (σ-scored Engram JSON; lab bookkeeping)",
    )
    idp.add_argument(
        "--path",
        type=str,
        default="",
        dest="identity_path",
        metavar="PATH",
        help="engram JSON path (default: ~/.cos/engram.json, or COS_ENGRAM_PATH if set)",
    )
    idp.set_defaults(func=_cmd_identity)

    initp = sub.add_parser(
        "init",
        help="Bootstrap a σ-gate project dir (cos_config.yaml, probes/, evals/, examples/, README)",
    )
    initp.add_argument("--name", type=str, required=True, dest="init_name", metavar="NAME")
    initp.add_argument(
        "--persona",
        type=str,
        default="enterprise",
        dest="init_persona",
        metavar="PERSONA",
        help="automotive|medical|creative|enterprise|research",
    )
    initp.add_argument("--dest", type=str, default=".", dest="init_dest", help="parent directory")
    initp.set_defaults(func=_cmd_init)

    gatep = sub.add_parser("gate", help="Score --prompt + --response with σ-gate (entropy core; pass probe_path for LSD)")
    gatep.add_argument("--prompt", type=str, required=True)
    gatep.add_argument("--response", type=str, required=True)
    gatep.add_argument("--json", action="store_true", dest="score_as_json", help="print JSON {\"sigma\", \"verdict\"} only")
    gatep.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    gatep.set_defaults(func=_cmd_gate_score)

    rea = sub.add_parser(
        "reason",
        help="Natlog-style backward chaining: facts/rules (colon args), σ on each fact + rule-head step",
    )
    rea.add_argument("--query", type=str, required=True, dest="reason_query", metavar="PRED:ARG:...")
    rea.add_argument(
        "--facts",
        nargs="*",
        default=[],
        dest="reason_facts",
        metavar="PRED:ARG:...",
        help="ground facts predicate:arg1:arg2 (repeatable)",
    )
    rea.add_argument("--json", action="store_true", dest="out_json", help="machine-readable output")
    rea.set_defaults(func=_cmd_reason_cli)

    scr = sub.add_parser("score", help="Alias of cos gate")
    scr.add_argument("--prompt", type=str, required=True)
    scr.add_argument("--response", type=str, required=True)
    scr.add_argument("--json", action="store_true", dest="score_as_json", help="print JSON {\"sigma\", \"verdict\"} only")
    scr.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    scr.set_defaults(func=_cmd_gate_score)

    bench_hint = sub.add_parser(
        "bench",
        help="Toy SigmaBench, --sigma-harness σ tuples, --mtier disclosure, --multi-model, or --resume",
    )
    bench_hint.add_argument(
        "--resume",
        type=str,
        default="",
        dest="bench_resume",
        help="continue eval from eval_results/checkpoint_<id>.jsonl (uses sidecar .meta.json)",
    )
    bench_hint.add_argument(
        "--multi-model",
        action="store_true",
        dest="bench_multi_model",
        help="evaluate σ-gate on OpenAI-compatible /v1 for each --models entry (checkpoint every 5 by default)",
    )
    bench_hint.add_argument(
        "--models",
        type=str,
        default="",
        dest="bench_models_list",
        help="comma-separated keys from cos.eval.multi_model_eval.EVAL_MODELS",
    )
    bench_hint.add_argument("--n", type=int, default=30, dest="bench_n", help="rows per model per dataset (default 30)")
    bench_hint.add_argument(
        "--endpoint",
        type=str,
        default="http://127.0.0.1:8000/v1",
        dest="bench_endpoint",
        help="OpenAI-compatible root (default local vLLM/SGLang)",
    )
    bench_hint.add_argument(
        "--checkpoint-every",
        type=int,
        default=5,
        dest="bench_checkpoint_every",
        help="JSONL append frequency for --multi-model (default 5)",
    )
    bench_hint.add_argument(
        "--output-dir",
        type=str,
        default="eval_results",
        dest="bench_output_dir",
        help="directory for checkpoint_*.jsonl and results_*.json",
    )
    bench_hint.add_argument(
        "--data-path",
        type=str,
        default="",
        dest="bench_data_path",
        help="optional JSONL path for TruthfulQA/SimpleQA/HaluEval loaders",
    )
    bench_hint.add_argument(
        "--mtier",
        action="store_true",
        dest="bench_mtier",
        help="print M-tier v2 table (positives + negatives + pending; use with --json for machine bundle)",
    )
    bench_hint.add_argument(
        "--sigma-harness",
        action="store_true",
        dest="bench_sigma_harness",
        help="run σ-gate on bundled (prompt, response, gold) tuples; honors --dataset --n --output-dir --checkpoint-every",
    )
    bench_hint.add_argument("--dataset", type=str, default="", dest="bench_dataset")
    bench_hint.add_argument(
        "--behavioral",
        action="store_true",
        dest="bench_behavioral",
        help="include behavioral calibration table fields when supported",
    )
    bench_hint.add_argument("--json", action="store_true", dest="out_json")
    bench_hint.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    bench_hint.set_defaults(func=_cmd_bench)

    chatp = sub.add_parser(
        "chat",
        help="σ-gated chat via OpenAI-compatible API (REPL or --prompt); use --offline for echo lab JSON",
    )
    chatp.add_argument(
        "--model",
        type=str,
        default="Qwen/Qwen3.6-35B-A3B",
        dest="chat_model",
        metavar="MODEL",
    )
    chatp.add_argument(
        "--endpoint",
        type=str,
        default="",
        dest="chat_endpoint",
        metavar="URL",
        help="OpenAI base URL (default http://localhost:8000/v1 or CREATION_OS_CHAT_ENDPOINT)",
    )
    chatp.add_argument(
        "--api-key",
        type=str,
        default="",
        dest="chat_api_key",
        metavar="KEY",
        help="API key (default OPENAI_API_KEY or 'local')",
    )
    chatp.add_argument(
        "--no-think",
        action="store_true",
        dest="chat_no_think",
        help="disable Qwen preserve_thinking chat_template hint",
    )
    chatp.add_argument(
        "--system",
        type=str,
        default="",
        dest="chat_system",
        metavar="TEXT",
        help="optional system message (first turn only)",
    )
    chatp.add_argument(
        "--offline",
        action="store_true",
        dest="chat_offline",
        help="lab JSON on stdout (no network); echo via local Pipeline stub",
    )
    chatp.add_argument(
        "--prompt",
        type=str,
        default="",
        dest="chat_prompt",
        help="single user turn; omit for interactive REPL",
    )
    chatp.add_argument("--json", action="store_true", dest="out_json")
    chatp.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    chatp.set_defaults(func=_cmd_chat)

    lrnp = sub.add_parser(
        "learn",
        help="σ-consolidation lab: anchor-gated steps (e.g. --forgetting-test on JSONL anchors)",
    )
    lrnp.add_argument(
        "--forgetting-test",
        action="store_true",
        dest="learn_forgetting_test",
        help="run ToyContinualModel anchor loop (see tests/test_sigma_consolidation_v134.py)",
    )
    lrnp.add_argument("--anchors", type=str, default="", dest="learn_anchors", metavar="PATH", help="JSONL anchors")
    lrnp.add_argument("--steps", type=int, default=100, dest="learn_steps")
    lrnp.add_argument("--lr", type=float, default=0.001, dest="learn_lr")
    lrnp.set_defaults(func=_cmd_learn)

    expl = sub.add_parser("explain", help="Why is σ at this value? (threshold bands + lite note)")
    expl.add_argument("--prompt", type=str, required=True, dest="explain_prompt")
    expl.add_argument("--response", type=str, required=True, dest="explain_response")
    expl.add_argument("--json", action="store_true", dest="out_json")
    expl.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    expl.set_defaults(func=_cmd_explain)

    casc = sub.add_parser("cascade", help="Print score_cascade dict (L1 + optional L2–L5 / LSD)")
    casc.add_argument("--prompt", type=str, required=True, dest="cascade_prompt")
    casc.add_argument("--response", type=str, required=True, dest="cascade_response")
    casc.add_argument("--json", action="store_true", dest="out_json")
    casc.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    casc.set_defaults(func=_cmd_cascade_cli)

    hlth = sub.add_parser("health", help="Python + optional dependency probe (offline)")
    hlth.add_argument("--json", action="store_true", dest="out_json")
    hlth.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    hlth.set_defaults(func=_cmd_health)

    offp = sub.add_parser(
        "offline",
        help="Air-gap connectivity heuristics (--verify: DNS name resolution + TCP probe + Fabric.boot; not formal certification)",
    )
    offp.add_argument("--verify", action="store_true", dest="offline_verify", help="run probes (see docs/AIRGAP.md)")
    offp.add_argument("--json", action="store_true", dest="out_json")
    offp.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    offp.set_defaults(func=_cmd_offline)

    hw = sub.add_parser(
        "hardware",
        help="Detect local RAM/GPU class and print conservative inference hints (see docs/HARDWARE_SETUP.md)",
    )
    hw.add_argument("--json", action="store_true", dest="out_json")
    hw.set_defaults(func=_cmd_hardware)

    lyr = sub.add_parser(
        "layers",
        help="L0–L9 module coverage after Fabric.boot() (FABRIC_LAYER_MAP; NOT AGI ACHIEVED)",
    )
    lyr.add_argument("--json", action="store_true", dest="out_json")
    lyr.set_defaults(func=_cmd_layers)

    regp = sub.add_parser("registry", help="List σ-MCP JSON registry entries (lab)")
    regp.add_argument("--list", action="store_true", dest="registry_list", help="print servers map")
    regp.add_argument(
        "--path",
        type=str,
        default="~/.cos/sigma_mcp_registry.json",
        dest="registry_path",
        metavar="PATH",
    )
    regp.add_argument("--json", action="store_true", dest="out_json")
    regp.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    regp.set_defaults(func=_cmd_registry_cli)

    cstp = sub.add_parser(
        "cost",
        help="σ-cost: CostManager session (summary|budget|reset) or legacy --report / sigma_cost --route",
    )
    cstp.add_argument(
        "cost_action",
        nargs="?",
        default=None,
        choices=["summary", "budget", "reset"],
        help="optional session action (omit for legacy --report / --route)",
    )
    cstp.add_argument(
        "--budget",
        type=float,
        default=10.0,
        dest="cost_manager_budget",
        metavar="USD",
        help="session budget for CostManager actions (default 10)",
    )
    cstp.add_argument("--route", action="store_true", dest="cost_route", help="cheapest σ-first routing JSON + --state snapshot")
    cstp.add_argument("--prompt", type=str, default="", dest="cost_route_prompt", help="with --route")
    cstp.add_argument("--models", type=str, default="", dest="cost_models", help="comma-separated model ids (with --route)")
    cstp.add_argument("--state", type=str, default="", dest="cost_state_path", help="write sigma_cost lab JSON (with --route)")
    cstp.add_argument("--report", action="store_true", dest="cost_report")
    cstp.add_argument("--dataset", type=str, default="lab", dest="cost_dataset")
    cstp.add_argument("--units", type=float, default=1.0, dest="cost_units")
    cstp.add_argument("--json", action="store_true", dest="out_json")
    cstp.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    cstp.set_defaults(func=_cmd_cost_cli)

    grp = sub.add_parser("graph", help="σ knowledge graph add or query (lab)")
    grp.add_argument("--add", nargs=3, metavar=("S", "R", "O"), dest="graph_add", help="add a triple")
    grp.add_argument("--sigma", type=float, default=None, dest="graph_sigma", help="optional σ for write gate")
    grp.add_argument("--hop-from", type=str, default="", dest="graph_hop_from")
    grp.add_argument("--hop-to", type=str, default="", dest="graph_hop_to")
    grp.add_argument("--hops", type=int, default=3, dest="graph_hops")
    grp.add_argument("--load-json", type=str, default="", dest="graph_load_json", help="load triples from export_json file")
    grp.add_argument(
        "--export",
        nargs=2,
        metavar=("MODE", "PATH"),
        dest="graph_export",
        default=None,
        help="export graph: json PATH | obsidian DIR",
    )
    grp.add_argument("--viz", type=str, default="", dest="graph_viz", help="render PNG (optional networkx+matplotlib)")
    grp.add_argument("--json", action="store_true", dest="out_json")
    grp.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    grp.set_defaults(func=_cmd_graph_cli)

    cau = sub.add_parser("causal", help="Causal DAG: causes/effects (see), do(), root-cause, what-if (lab)")
    cau.add_argument(
        "causal_action",
        choices=["causes", "effects", "do", "why", "what-if"],
        metavar="ACTION",
    )
    cau.add_argument("--var", type=str, default="", dest="causal_var", help="variable (entity name)")
    cau.add_argument("--target", type=str, default="", dest="causal_target", help="outcome variable (what-if)")
    cau.add_argument(
        "--observed",
        type=str,
        default="{}",
        dest="causal_observed",
        help='what-if: JSON object of observed assignments (e.g. {"smoke": 1})',
    )
    cau.add_argument(
        "--intervene",
        type=str,
        default="{}",
        dest="causal_intervene",
        help='what-if: JSON intervention dict (e.g. {"fire": 0})',
    )
    cau.add_argument(
        "--load-graph",
        type=str,
        default="",
        dest="causal_load_json",
        help="load SigmaGraph JSON export as causal edges (caus*/leads_to relations)",
    )
    cau.add_argument("--json", action="store_true", dest="out_json")
    cau.set_defaults(func=_cmd_causal_cli)

    wrd = sub.add_parser(
        "world",
        help="Heuristic σ-JEPA world model over --observe sequence (numpy optional; lab)",
    )
    wrd.add_argument(
        "--observe",
        nargs="+",
        required=True,
        dest="world_observe",
        metavar="TEXT",
        help="one or more observation strings (latent prediction σ vs realization)",
    )
    wrd.add_argument("--dim", type=int, default=256, dest="world_dim", help="latent dimension (default 256)")
    wrd.add_argument("--json", action="store_true", dest="out_json", help="emit JSON summary only")
    wrd.set_defaults(func=_cmd_world_cli)

    drm = sub.add_parser("dream", help="σ graph maintenance (dedup, decay, infer, orphans, σ report)")
    drm.add_argument("--load-json", type=str, default="", dest="dream_load_json")
    drm.add_argument("--add", nargs=3, metavar=("S", "R", "O"), dest="dream_add", default=None)
    drm.add_argument("--dedup-threshold", type=float, default=0.93, dest="dream_dedup_threshold")
    drm.add_argument("--max-age-days", type=float, default=30.0, dest="dream_max_age_days")
    drm.add_argument("--no-decay", action="store_true", dest="dream_no_decay")
    drm.add_argument("--no-dedup", action="store_true", dest="dream_no_dedup")
    drm.add_argument("--no-orphans", action="store_true", dest="dream_no_orphans")
    drm.add_argument("--no-infer", action="store_true", dest="dream_no_infer")
    drm.add_argument(
        "--with-memory",
        action="store_true",
        dest="dream_with_memory",
        help="also run SigmaMemory consolidate() + decay() (same gate/graph)",
    )
    drm.add_argument("--json", action="store_true", dest="out_json")
    drm.set_defaults(func=_cmd_dream_cli)

    ing = sub.add_parser(
        "ingest",
        help="Ingest document into σ-graph (word chunks, relation-regex or LLM extract, σ filter)",
    )
    ing.add_argument("ingest_path", nargs="?", default="", help="file path (.txt .md .html .docx .pdf .epub)")
    ing.add_argument("--load-json", type=str, default="", dest="ingest_load_json", help="start from existing GraphExport JSON")
    ing.add_argument("--save-json", type=str, default="", dest="ingest_save_json", help="write graph after ingest (GraphExport JSON)")
    ing.add_argument("--json", action="store_true", dest="out_json")
    ing.set_defaults(func=_cmd_ingest_cli)

    ragp = sub.add_parser(
        "rag",
        help="σ-RAG lab: word-chunk ingest + persist, retrieve with σ gate rerank, stats",
    )
    ragp.add_argument(
        "rag_action",
        choices=["ingest", "query", "stats"],
        help="ingest (--file), query (--query), or stats",
    )
    ragp.add_argument("--file", type=str, default="", dest="rag_file", metavar="PATH", help="document path (ingest)")
    ragp.add_argument("--query", type=str, default="", dest="rag_query", metavar="TEXT", help="question (query)")
    ragp.add_argument(
        "--store-dir",
        type=str,
        default="",
        dest="rag_store_dir",
        metavar="DIR",
        help="chunk store directory (default ~/.cos/rag)",
    )
    ragp.set_defaults(func=_cmd_rag)

    voi = sub.add_parser(
        "voice",
        help="Local voice lab: σ on transcript + before speak (optional faster-whisper / kokoro)",
    )
    voi.add_argument(
        "voice_action",
        nargs="?",
        default=None,
        choices=["transcribe", "speak", "chat", "status", "listen"],
        help="optional: transcribe | speak | chat | status (else use legacy flags)",
    )
    voi.add_argument("--input", type=str, default="", dest="voice_input", metavar="PATH", help="audio path for transcribe/chat")
    voi.add_argument("--text", type=str, default="", dest="voice_text", metavar="STR", help="text for speak action")
    voi.add_argument("--listen-mock", type=str, default="", dest="voice_mock", metavar="TEXT")
    voi.add_argument("--audio", type=str, default="", dest="voice_audio", metavar="PATH")
    voi.add_argument("--speak", type=str, default="", dest="voice_speak", metavar="TEXT")
    voi.add_argument("--check", type=str, default="", dest="voice_check", metavar="TEXT")
    voi.add_argument("--model", type=str, default="tiny", dest="voice_whisper_model", help="faster-whisper model id")
    voi.add_argument("--voice", type=str, default="af_bella", dest="voice_kokoro_voice", help="Kokoro voice id (TTS)")
    voi.add_argument("--json", action="store_true", dest="out_json")
    voi.set_defaults(func=_cmd_voice_cli)

    uip = sub.add_parser("ui", help="Launch σ-dashboard (NiceGUI; pip install 'creation-os[ui]')")
    uip.add_argument("--host", type=str, default="127.0.0.1", dest="ui_host")
    uip.add_argument("--port", type=int, default=8080, dest="ui_port")
    uip.add_argument("--headless", action="store_true", dest="ui_headless", help="do not auto-open browser")
    uip.add_argument(
        "--native",
        action="store_true",
        dest="ui_native",
        help="native desktop window (pywebview) when supported",
    )
    uip.set_defaults(func=_cmd_ui_cli)

    hdc = sub.add_parser(
        "hdc",
        help="BSC-style hypervectors: encode triple, query unbind, sequence σ (pip install numpy)",
    )
    hdc_sub = hdc.add_subparsers(dest="hdc_cmd", required=True)
    hdc_enc = hdc_sub.add_parser("encode", help="encode subject–relation–object superposition")
    hdc_enc.add_argument("--subject", type=str, required=True, dest="hdc_s")
    hdc_enc.add_argument("--relation", type=str, required=True, dest="hdc_r")
    hdc_enc.add_argument("--object", type=str, required=True, dest="hdc_o")
    hdc_enc.add_argument("--dim", type=int, default=10_000, dest="hdc_dim")
    hdc_enc.add_argument("--seed", type=int, default=0, dest="hdc_seed")
    hdc_enc.set_defaults(func=_cmd_hdc_cli)
    hdc_q = hdc_sub.add_parser("query", help="similarity of unbind to permuted object vector")
    hdc_q.add_argument("--subject", type=str, required=True, dest="hdc_s")
    hdc_q.add_argument("--relation", type=str, required=True, dest="hdc_r")
    hdc_q.add_argument("--object", type=str, required=True, dest="hdc_o")
    hdc_q.add_argument("--dim", type=int, default=10_000, dest="hdc_dim")
    hdc_q.add_argument("--seed", type=int, default=0, dest="hdc_seed")
    hdc_q.set_defaults(func=_cmd_hdc_cli)
    hdc_seq = hdc_sub.add_parser("sequence", help="σ-gated lab score for a token sequence")
    hdc_seq.add_argument("--tokens", type=str, required=True, dest="hdc_sequence", metavar="CSV", help="comma-separated tokens")
    hdc_seq.add_argument("--dim", type=int, default=10_000, dest="hdc_dim")
    hdc_seq.add_argument("--seed", type=int, default=0, dest="hdc_seed")
    hdc_seq.set_defaults(func=_cmd_hdc_cli)

    evo = sub.add_parser("evolve", help="σ-evolve lab loop (single improve_loop step by default)")
    evo.add_argument(
        "--target",
        type=str,
        default="",
        dest="evolve_target",
        metavar="PATH",
        help="v137 evolution target module (use with --goal; immutable paths rejected)",
    )
    evo.add_argument("--goal", type=str, default="", dest="evolve_goal", help="improvement objective (with --target)")
    evo.add_argument("--step", action="store_true", dest="evolve_step", help="run one bounded improve loop")
    evo.add_argument(
        "--steps",
        type=int,
        default=0,
        dest="evolve_rsi_steps",
        metavar="N",
        help="RSI σ-gate lab loop length (0 = disabled)",
    )
    evo.add_argument("--iters", type=int, default=1, dest="evolve_iters")
    evo.add_argument("--json", action="store_true", dest="out_json")
    evo.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    evo.set_defaults(func=_cmd_evolve_step)

    rtp = sub.add_parser(
        "redteam",
        help="Adversarial σ-gate: --campaign false-accept hunt, or --target mock (sigma_red_team lab)",
    )
    rtp.add_argument(
        "--campaign",
        action="store_true",
        dest="redteam_gate_campaign",
        help="false-accept hunt on σ-gate with template attacks (no LLM; --per-case caps variants)",
    )
    rtp.add_argument(
        "--per-case",
        type=int,
        default=5,
        dest="redteam_per_case",
        metavar="N",
        help="max attack templates per benchmark row when using --campaign (default 5)",
    )
    rtp.add_argument("--target", type=str, default="mock", dest="redteam_target")
    rtp.add_argument("--attacks", type=int, default=10, dest="redteam_attacks")
    rtp.add_argument("--json", action="store_true", dest="out_json")
    rtp.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    rtp.set_defaults(func=_cmd_redteam_cli)

    fab = sub.add_parser("fabric", help="σ-fabric: boot layers + traced process() (lab orchestration)")
    fab_sub = fab.add_subparsers(dest="fabric_cmd", required=True)
    fab_st = fab_sub.add_parser("status", help="Print booted layer map as JSON")
    fab_st.set_defaults(func=_cmd_fabric)
    fab_pr = fab_sub.add_parser("process", help="Run fabric.process with --prompt and optional --response")
    fab_pr.add_argument("--prompt", type=str, required=True, dest="fabric_prompt")
    fab_pr.add_argument("--response", type=str, default="", dest="fabric_response")
    fab_pr.set_defaults(func=_cmd_fabric)

    bootp = sub.add_parser(
        "boot",
        help="Startup: kernel σ-check, engram, Fabric, Mega, cascade (partial boot is OK; NOT AGI ACHIEVED)",
    )
    bootp.add_argument("--json", action="store_true", dest="out_json", help="machine-readable output")
    bootp.set_defaults(func=_cmd_cognitive_boot)

    cstat = sub.add_parser(
        "status",
        help="Cognitive snapshot JSON (module diagnostics + metacognition / awareness_metrics)",
    )
    cstat.set_defaults(func=_cmd_cognitive_status)

    cproc = sub.add_parser("process", help="One cognitive Fabric step (Ω-loop or gate-only fallback)")
    cproc.add_argument("cognitive_input", type=str, help="input text / goal")
    cproc.set_defaults(func=_cmd_cognitive_process)

    memp = sub.add_parser(
        "memory",
        help="Three-tier σ memory: store, recall, stats, consolidate, forget",
    )
    memp.add_argument(
        "mem_action",
        choices=["store", "recall", "stats", "consolidate", "forget"],
        metavar="action",
    )
    memp.add_argument("--content", type=str, default="", dest="mem_content")
    memp.add_argument("--context", type=str, default="", dest="mem_context")
    memp.add_argument("--query", type=str, default="", dest="mem_query")
    memp.add_argument(
        "--type",
        type=str,
        default="episodic",
        dest="mem_type",
        metavar="TYPE",
        help="tier for store: working|episodic|semantic|procedural",
    )
    memp.add_argument(
        "--persist-dir",
        type=str,
        default="",
        dest="mem_persist_dir",
        help="directory for memories.json (default ~/.cos/memory)",
    )
    memp.set_defaults(func=_cmd_memory_cli)

    pip = sub.add_parser(
        "pipe",
        help="σ pipeline: input guard → generate or --response → σ-gate → output guard",
    )
    pip.add_argument("--prompt", type=str, required=True, dest="pipe_prompt")
    pip.add_argument(
        "--response",
        type=str,
        default="",
        dest="pipe_response",
        help="if set, score this text instead of calling a model",
    )
    pip.set_defaults(func=_cmd_pipe)

    st = sub.add_parser("stats", help="Print default Pipeline stats as JSON")
    st.set_defaults(func=_cmd_stats)

    stream_p = sub.add_parser(
        "stream",
        help="Per-token σ replay (score-only): entropy + repeat + length signals (no live LLM)",
    )
    stream_p.add_argument("--prompt", type=str, required=True, dest="stream_prompt")
    stream_p.add_argument("--interrupt-threshold", type=float, default=0.8)
    stream_p.add_argument(
        "--tokens",
        type=str,
        default="",
        dest="stream_tokens",
        help="space-separated tokens (default: short demo phrase)",
    )
    stream_p.set_defaults(func=_cmd_stream)

    snap_p = sub.add_parser(
        "snapshot",
        help="Checkpoint and rollback σ-gate EMA/counters + pipeline stats (lab JSON under ./snapshots)",
    )
    snap_p.add_argument(
        "snapshot_action",
        choices=["save", "rollback", "list", "diff"],
        metavar="action",
    )
    snap_p.add_argument("--label", default=None, dest="snapshot_label")
    snap_p.set_defaults(func=_cmd_snapshot)

    cal_p = sub.add_parser(
        "calibrate",
        help="Fit or evaluate σ→P(error) calibration; 'lab' runs toy behavioral table",
    )
    cal_p.add_argument(
        "calibrate_action",
        choices=["fit", "ece", "report", "lab"],
        metavar="action",
    )
    cal_p.add_argument("--data", type=str, default="", dest="calibrate_data", help="validation pairs JSON")
    cal_p.add_argument(
        "--dataset",
        type=str,
        default="",
        dest="calibrate_dataset",
        help="for lab: TruthfulQA | TriviaQA | HaluEval | MMLU | HellaSwag",
    )
    cal_p.add_argument("--method", default="platt", choices=["platt", "isotonic"], dest="calibrate_method")
    cal_p.add_argument("--out", type=str, default="calibration.json", dest="calibrate_out", help="fit output JSON")
    cal_p.add_argument("--model", type=str, default="", dest="calibrate_model", help="calibration JSON (ece/report)")
    cal_p.add_argument("--bins", type=int, default=10, dest="calibrate_bins", help="ECE / reliability bins")
    cal_p.add_argument("--json", action="store_true", dest="out_json")
    cal_p.add_argument("-v", "--verbose", action="store_true", dest="cli_verbose")
    cal_p.set_defaults(func=_cmd_calibrate)

    distill = sub.add_parser("distill", help="σ-filtered distillation helpers")
    dsub = distill.add_subparsers(dest="distill_cmd", required=True)

    g = dsub.add_parser("generate", help="teacher → σ-filter → JSONL")
    g.add_argument("--teacher", type=str, default="", help="label only unless harness wired")
    g.add_argument("--prompts", type=str, required=True, help="input .jsonl (prompt/text/instruction keys)")
    g.add_argument("--output", type=str, required=True, help="output .jsonl")
    g.add_argument("--limit", type=int, default=10_000)
    g.add_argument("--k-raw", type=float, default=0.95, dest="k_raw")
    g.add_argument("--mock", action="store_true", help="use mock teacher + constant σ gate")
    g.add_argument("--mock-sigma", type=float, default=0.18, help="σ used with --mock (stable ACCEPT band)")
    g.set_defaults(func=_cmd_distill_generate)

    t = dsub.add_parser("train", help="student train_step over clean JSONL")
    t.add_argument("--student", type=str, default="", help="label only (harness wiring)")
    t.add_argument("--data", type=str, required=True, help="clean_train.jsonl from generate")
    t.add_argument("--epochs", type=int, default=3)
    t.set_defaults(func=_cmd_distill_train)

    ev = dsub.add_parser("eval", help="evaluation stub")
    ev.add_argument("--student", type=str, default="")
    ev.add_argument("--benchmark", type=str, default="truthfulqa")
    ev.set_defaults(func=_cmd_distill_eval)

    deb = sub.add_parser("debate", help="σ-scored two-deputy debate (mock or harness)")
    deb.add_argument("--models", type=str, required=True, help='comma-separated labels, e.g. "a,b"')
    deb.add_argument("--question", type=str, required=True)
    deb.add_argument("--rounds", type=int, default=3)
    deb.add_argument("--mock", action="store_true", help="run skewed mock deputies + σ gate")
    deb.set_defaults(func=_cmd_debate)

    sp = sub.add_parser("self-play", help="σ self-play over prompts (mock or harness)")
    sp.add_argument("--model", type=str, default="", help="label only unless harness wired")
    sp.add_argument("--prompts", type=str, default="", help="optional JSONL of prompts")
    sp.add_argument("--question", type=str, default="", help="single prompt if no --prompts")
    sp.add_argument("--output", type=str, default="", help="optional JSONL output path")
    sp.add_argument("--mock", action="store_true")
    sp.set_defaults(func=_cmd_self_play)

    pr = sub.add_parser(
        "proconductor",
        help="Spektre node stack lab + legacy four-deputy σ consensus (mock or harness)",
    )
    pr.add_argument("--question", type=str, default="", help="legacy four-deputy mock question (with --mock)")
    pr.add_argument("--all-models", action="store_true", dest="all_models", help="reserved for harness wiring")
    pr.add_argument("--mock", action="store_true")
    pr.add_argument("--stack", action="store_true", help="print node stack summary from --node-stack (default bundled JSON)")
    pr.add_argument("--task", type=str, default="", help="node stack task string (with --triangulate / --auto-route)")
    pr.add_argument("--auto-route", action="store_true", dest="auto_route", help="route --task and run stub execute_with_sigma")
    pr.add_argument("--triangulate", action="store_true", help="triangulate --task on railo+niko+deepseek stubs")
    pr.add_argument("--firmware-scan", action="store_true", dest="firmware_scan", help="run firmware heuristics on --sample-text for --node")
    pr.add_argument("--node", type=str, default="", help="node id for --firmware-scan")
    pr.add_argument("--sample-text", type=str, default="", dest="sample_text", help="text for --firmware-scan")
    pr.add_argument(
        "--node-stack",
        type=str,
        default="",
        dest="node_stack",
        metavar="PATH",
        help="path to node_stack.yaml or .json (default: configs/node_stack.json)",
    )
    pr.set_defaults(func=_cmd_proconductor)

    om = sub.add_parser("omega", help="Ω-loop harness (14 σ phases per turn; lab scaffold)")
    om.add_argument("--goal", type=str, default="", help="task / objective (required for harness; optional with --step)")
    om.add_argument("--turns", type=int, default=50, help="maximum Ω turns (default 50)")
    om.add_argument(
        "--step",
        action="store_true",
        dest="omega_cognitive_step",
        help="single cognitive Ω step (σ-gate + memory + graph lab path)",
    )
    om.add_argument("--mock", action="store_true", help="reserved for mock backends in harness wiring")
    om.add_argument("--json", action="store_true", help="print full turn history as JSON")
    om.set_defaults(func=_cmd_omega)

    mon = sub.add_parser("monitor", help="σ-native monitor (HTML dashboard; local-first)")
    mon.add_argument("--html", action="store_true", help="emit self-contained HTML (stdout unless --output)")
    mon.add_argument("--output", type=str, default="", help="write HTML to this path instead of stdout")
    mon.add_argument("--from-file", type=str, default="", dest="from_file", help="JSON array, JSONL, or object with steps/traces")
    mon.add_argument("--mock-drift", action="store_true", dest="mock_drift", help="built-in drifting σ fixture")
    mon.set_defaults(func=_cmd_monitor)

    ob = sub.add_parser(
        "observe",
        help="σ observability: default dashboard from ~/.cos/logs; or trace JSON via --from-file",
    )
    ob.add_argument("--last", type=int, default=100, dest="observe_last", help="summary over last N rows from today's log")
    ob.add_argument(
        "--log-dir",
        type=str,
        default="~/.cos/logs",
        dest="observe_log_dir",
        help="directory for sigma_YYYY-MM-DD.jsonl logs",
    )
    ob.add_argument("--json", action="store_true", dest="out_json", help="machine-readable summary (dashboard mode)")
    ob.add_argument("--traces", action="store_true", help="(legacy) include normalized σ-traces from --from-file")
    ob.add_argument("--alerts", action="store_true", help="(legacy) include σ-alert list")
    ob.add_argument("--from-file", type=str, default="", dest="from_file", help="JSON / JSONL same as cos monitor")
    ob.add_argument("--mock-drift", action="store_true", dest="mock_drift", help="synthetic drift for CI / demos")
    ob.set_defaults(func=_cmd_observe)

    dr = sub.add_parser(
        "drift",
        help="σ distribution drift vs baseline (JSON arrays of floats)",
    )
    dr.add_argument("--baseline", type=str, required=True, dest="drift_baseline", help="JSON file: [σ, ...]")
    dr.add_argument("--current", type=str, required=True, dest="drift_current", help="JSON file: [σ, ...]")
    dr.add_argument("--json", action="store_true", dest="out_json", help="print detection payload only")
    dr.set_defaults(func=_cmd_drift)

    _fed_p = argparse.ArgumentParser(add_help=False)
    _fed_p.add_argument("--workspace", type=str, default="~/.cos/federation", help="state directory")
    _fed_p.add_argument(
        "--aggregate",
        action="store_true",
        dest="federated_aggregate",
        help="run in-memory aggregate demo; writes fed_v161_stats.json under --workspace",
    )
    _fed_p.add_argument("--train", action="store_true", help="run local toy σ-FL rounds")
    _fed_p.add_argument("--rounds", type=int, default=3, help="training rounds (with --train)")
    _fed_p.add_argument("--no-poison", action="store_true", dest="no_poison", help="omit poison toy node")
    _fed_p.add_argument("--no-byzantine", action="store_true", dest="no_byzantine", help="skip median outlier filter")
    _fed_p.add_argument("--status", action="store_true", help="print fed_lab_state.json from workspace")
    _fed_p.add_argument("--server", action="store_true", help="serve GET /status POST /train POST /join")
    _fed_p.add_argument("--host", type=str, default="127.0.0.1", help="bind address (with --server)")
    _fed_p.add_argument("--port", type=int, default=8080, help="TCP port (with --server)")
    _fed_p.add_argument("--join", action="store_true", help="POST /join to --server-url")
    _fed_p.add_argument("--server-url", type=str, default="", dest="server_url", help="e.g. http://127.0.0.1:8080")
    _fed_p.add_argument("--data", type=str, default="", help="local data path label (with --join)")

    fed = sub.add_parser(
        "federation",
        parents=[_fed_p],
        help="σ-federated lab (local train + optional stdlib HTTP; share σ-screened updates)",
    )
    fed.set_defaults(func=_cmd_federation)
    fed_alt = sub.add_parser(
        "federated",
        parents=[_fed_p],
        help="alias of cos federation (same flags; used by C cos shim and v161 tests)",
    )
    fed_alt.set_defaults(func=_cmd_federation)

    ex = sub.add_parser("exec", help="σ digital twin / sandbox pre-execution (simulate before run)")
    ex.add_argument("--workspace", type=str, default="~/.cos/exec", help="checkpoint dir for rollback")
    ex.add_argument("--simulate", type=str, default="", metavar="CMD", help="dry-run σ + safety on CMD")
    ex.add_argument("--sandbox", type=str, default="", metavar="CMD", help="allowlisted subprocess sandbox")
    ex.add_argument("--twin", type=str, default="", metavar="CMD", help="simulate then execute if σ allows")
    ex.add_argument(
        "--with-rollback",
        action="store_true",
        dest="with_rollback",
        help="with --twin: rollback if sim–real gap exceeds threshold",
    )
    ex.add_argument("--gap-threshold", type=float, default=0.3, dest="gap_threshold")
    ex.add_argument("--rollback", action="store_true", help="restore last JSON checkpoint from workspace")
    ex.set_defaults(func=_cmd_exec)

    sim = sub.add_parser("simulate", help="alias: same as cos exec --simulate CMD")
    sim.add_argument("command", type=str, help="shell-shaped command string to simulate")
    sim.add_argument("--workspace", type=str, default="~/.cos/exec")
    sim.set_defaults(func=_cmd_simulate)

    prv = sub.add_parser("prove", help="σ-gate receipts + hash-chain verify (lab; not a succinct ZK SNARK)")
    prv.add_argument("--prompt", type=str, default="", help="plaintext prompt (hashed; not stored in receipt)")
    prv.add_argument("--response", type=str, default="", help="plaintext response (hashed)")
    prv.add_argument("--sigma", type=float, default=-1.0, help="probe σ in [0,1] for gate update")
    prv.add_argument("--k-raw", type=float, default=0.92, dest="k_raw")
    prv.add_argument("--no-warm", action="store_true", dest="no_warm", help="skip 0.99 σ warm-start")
    prv.add_argument("--out", type=str, default="", help="write receipt JSON to this path")
    prv.add_argument("--verify", type=str, default="", help="verify a single receipt JSON file")
    prv.add_argument("--verify-chain", type=str, default="", dest="verify_chain", help="verify JSONL hash chain + receipts")
    prv.add_argument("--export-chain", action="store_true", dest="export_chain", help="copy --chain-file to --output")
    prv.add_argument("--chain-file", type=str, default="", dest="chain_file", help="input JSONL for --export-chain")
    prv.add_argument("--output", type=str, default="", help="output path (export-chain)")
    prv.set_defaults(func=_cmd_prove)

    vlab = sub.add_parser(
        "verify",
        help="lab SHA-256 σ commitment demo (integrity receipt; not cos prove / cos zkp succinct proof)",
    )
    vlab.add_argument("--json", action="store_true", dest="out_json", help="machine-readable output")
    vlab.set_defaults(func=_cmd_verify_commitment_lab)

    cst = sub.add_parser("constitution", help="σ constitution probe vs SigmaGate() (declarative lab checks)")
    cst.add_argument("--json", action="store_true", dest="out_json", help="machine-readable output")
    cst.set_defaults(func=_cmd_constitution)

    spc = sub.add_parser(
        "space-check",
        help="Heuristic AST checks (Power-of-10 *themes*); see docs/SPACE_GRADE.md — not qualification",
    )
    spc.add_argument("--file", type=str, default="", dest="space_check_file", metavar="PATH")
    spc.add_argument(
        "--module-dir",
        type=str,
        default="",
        dest="space_check_module_dir",
        metavar="DIR",
        help="directory to scan (default: cos package dir)",
    )
    spc.set_defaults(func=_cmd_space_check)

    mega = sub.add_parser(
        "mega",
        help="Unified σ cognitive sweep (Mega.step) or --fabric for legacy Fabric demo (NOT AGI ACHIEVED)",
    )
    mega.add_argument(
        "mega_text",
        metavar="OBSERVATION",
        help="observation text (passed to Mega.step; Fabric INPUT when --fabric)",
    )
    mega.add_argument("--goal", type=str, default=None, dest="mega_goal", help="optional goal string")
    mega.add_argument("--cycles", type=int, default=1, dest="mega_cycles", help="repeat Mega.step; dream + status when >1")
    mega.add_argument(
        "--fabric",
        action="store_true",
        dest="mega_fabric",
        help="legacy: Fabric boot + layer bars + process + constitution",
    )
    mega.add_argument("--json", action="store_true", dest="out_json", help="machine-readable output")
    mega.set_defaults(func=_cmd_mega)

    mcp = sub.add_parser(
        "mcp",
        help="σ-gate FastMCP server (default) or registry / wrap / register (see docs/MCP.md)",
    )
    mcp.add_argument(
        "--transport",
        choices=["stdio", "http", "streamable-http"],
        default="stdio",
        help="MCP transport: stdio | http | streamable-http (streamable HTTP on 127.0.0.1)",
    )
    mcp.add_argument(
        "--host",
        type=str,
        default="127.0.0.1",
        help="bind address when --transport http / streamable-http (default: 127.0.0.1)",
    )
    mcp.add_argument(
        "--port",
        type=int,
        default=8000,
        help="TCP port when --transport http (default: 8000)",
    )
    mcp.add_argument(
        "--list",
        action="store_true",
        dest="list_servers",
        help="print registered servers as JSON lines (trust, calls, abstain_rate)",
    )
    mcp.add_argument(
        "--registry",
        type=str,
        default="~/.cos/sigma_mcp_registry.json",
        help="JSON registry path (default: ~/.cos/sigma_mcp_registry.json)",
    )
    mcp.add_argument(
        "--wrap",
        action="store_true",
        dest="wrap_hint",
        help="print JSON note for wrapping a subprocess MCP server with σ-middleware",
    )
    mcp.add_argument("--server", type=str, default="", dest="wrap_server_cmd", metavar="CMD", help="command string for --wrap")
    mcp.add_argument("--register", action="store_true", dest="mcp_register", help="persist --server-id into --registry")
    mcp.add_argument("--server-id", type=str, default="", dest="server_id", metavar="ID")
    mcp.add_argument("--server-url", type=str, default="", dest="server_url", metavar="URL")
    mcp.add_argument("--metadata-json", type=str, default="", dest="metadata_json", metavar="JSON")
    mcp.set_defaults(func=_cmd_mcp)

    a2a = sub.add_parser("a2a", help="Agent-to-agent σ-trust checks (local lab; no network transport)")
    a2a.add_argument("--send", action="store_true", dest="a2a_send", help="score and deliver a message to a local peer")
    a2a.add_argument("--from", type=str, default="agent_1", dest="from_agent", metavar="AGENT", help="sender id (registry trust)")
    a2a.add_argument("--to", type=str, default="agent_2", dest="to_agent", metavar="AGENT", help="receiver id (acceptance policy)")
    a2a.add_argument("--message", type=str, default="", help="plaintext body (σ from lab encoder)")
    a2a.add_argument(
        "--registry",
        type=str,
        default="~/.cos/sigma_mcp_registry.json",
        help="JSON registry path shared with cos mcp",
    )
    a2a.set_defaults(func=_cmd_a2a)

    sw = sub.add_parser("swarm", help="σ-stigmergy + quorum swarm lab (mock agents; optional JSON persistence)")
    sw.add_argument("--agents", type=int, default=0, help="number of mock agents (with --task)")
    sw.add_argument("--task", type=str, default="", help="task goal string")
    sw.add_argument("--quorum", type=int, default=3, help="minimum agreeing agents for consensus")
    sw.add_argument("--domain", type=str, default="lab", help="stigmergy key prefix segment")
    sw.add_argument("--task-id", type=str, default="t1", dest="task_id", help="stigmergy key id segment")
    sw.add_argument("--env", action="store_true", help="inspect or decay persisted stigmergy table")
    sw.add_argument("--show", action="store_true", dest="env_show", help="with --env: print JSON snapshot")
    sw.add_argument(
        "--decay",
        type=float,
        default=None,
        dest="decay_rate",
        metavar="RATE",
        help="with --env: subtract RATE from each signal strength and prune dead rows",
    )
    sw.add_argument(
        "--env-file",
        type=str,
        default="~/.cos/swarm_stigmergy.json",
        dest="env_file",
        help="JSON persistence path (default ~/.cos/swarm_stigmergy.json)",
    )
    sw.add_argument(
        "--no-persist",
        action="store_true",
        dest="no_persist",
        help="in-memory stigmergy only (disables --env cross-run; OK for one-shot --agents)",
    )
    sw.set_defaults(func=_cmd_swarm)

    sov = sub.add_parser(
        "sovereign",
        help="σ-sovereign autonomy brake + σ-circuit-breaker + containment lab (local JSON state)",
    )
    sov.add_argument("--goal", type=str, default="", help="goal string for a single step or simulation")
    sov.add_argument("--max-actions", type=int, default=20, dest="max_actions", help="sovereign max_actions budget")
    sov.add_argument(
        "--simulate-steps",
        type=int,
        default=0,
        dest="simulate_steps",
        metavar="N",
        help="run N mock σ steps (rising probe + cascade tail) then apply autonomy to a delete action",
    )
    sov.add_argument("--status", action="store_true", help="print sovereign + circuit JSON snapshot")
    sov.add_argument("--circuit", action="store_true", dest="circuit_status", help="alias of --status (circuit included)")
    sov.add_argument("--halt", action="store_true", help="latch sovereign HALT in state file")
    sov.add_argument(
        "--state-file",
        type=str,
        default="~/.cos/sovereign_lab.json",
        dest="state_file",
        help="JSON persistence path (default ~/.cos/sovereign_lab.json)",
    )
    sov.set_defaults(func=_cmd_sovereign)

    rv = sub.add_parser(
        "resolve",
        help="σ-conflict resolution + node state machine + kernel lock (lab; no CBS search)",
    )
    rv.add_argument("--task", type=str, default="", help="task string (used with --nodes for deterministic stub rows)")
    rv.add_argument(
        "--nodes",
        type=str,
        default="",
        help='comma-separated node ids, e.g. "railo,niko,deepseek"',
    )
    rv.add_argument(
        "--states",
        action="store_true",
        dest="resolve_states",
        help="print NodeStateMachine.STATES as JSON",
    )
    rv.add_argument(
        "--verify-trace",
        type=str,
        default="",
        dest="verify_trace",
        metavar="TRACE",
        help="comma-separated states from IDLE (leading IDLE optional; e.g. IDLE,ACTIVE,PROCESSING,DONE)",
    )
    rv.add_argument(
        "--kernel-lock",
        action="store_true",
        dest="kernel_lock_cmd",
        help="print invariant list, kernel hash, and a demo successful lock envelope",
    )
    rv.set_defaults(func=_cmd_resolve)

    rep = sub.add_parser("report", help="σ audit trail helpers (local JSONL)")
    rep.add_argument("--agent", action="store_true", help="print recent agent audit lines")
    rep.add_argument(
        "--firewall",
        action="store_true",
        help="print σ-firewall / defence-stack counts from recent audit JSONL",
    )
    rep.add_argument(
        "--audit-dir",
        type=str,
        default="~/.cos/audit",
        help="directory for daily *.jsonl audit files",
    )
    rep.add_argument("--tail", type=int, default=80, help="max lines when using --agent")
    rep.set_defaults(func=_cmd_report)

    fw = sub.add_parser("firewall", help="σ-firewall lab checks (encode + σ-gate; no live LLM)")
    fw.add_argument("--check", type=str, default=None, help="classify user/tool-param shaped text")
    fw.add_argument("--check-tool", type=str, default=None, dest="check_tool", help="classify tool output shaped text")
    fw.set_defaults(func=_cmd_firewall)

    tsf = sub.add_parser(
        "tool-safety",
        help="Deterministic tool policy + σ-before-execute (SAFE/MODERATE/BLOCKED + SigmaGate)",
    )
    tsf.add_argument("--tool", type=str, required=True, dest="ts_tool", metavar="NAME", help="tool name")
    tsf.add_argument("--args", type=str, default="", dest="ts_args", metavar="STR", help="argument string")
    tsf.add_argument("--intent", type=str, default="", dest="ts_intent", metavar="STR", help="optional intent text for σ prompt")
    tsf.set_defaults(func=_cmd_tool_safety)

    ttt = sub.add_parser(
        "ttt",
        help="σ-governed test-time training lab (JSON state; --learn needs torch)",
    )
    ttt.add_argument("--prompt", type=str, default="", help="prompt text (with --learn)")
    ttt.add_argument("--context", type=str, default="", help="optional context string (with --learn)")
    ttt.add_argument("--learn", action="store_true", help="run lab σ-TTT step and append adaptation history")
    ttt.add_argument("--track", action="store_true", help="print recent Δσ summary from state file")
    ttt.add_argument("--last", type=int, default=20, help="window size for --track (default 20)")
    ttt.add_argument("--weights", action="store_true", help="print living-weight counters from state file")
    ttt.add_argument("--rollback", action="store_true", help="pop last history row and decrement RETHINK counter")
    ttt.add_argument(
        "--state-file",
        type=str,
        default="~/.cos/ttt_lab.json",
        dest="state_file",
        help="JSON persistence path (default ~/.cos/ttt_lab.json)",
    )
    ttt.set_defaults(func=_cmd_ttt)

    pred = sub.add_parser(
        "predict",
        help="σ-JEPA world model lab (latent predict / imagine / plan; no pixel decoder)",
    )
    pred.add_argument("--observation", type=str, default="", help="current observation string (single-step / imagine)")
    pred.add_argument("--action", type=str, default=None, help="optional action for conditioned one-step predict")
    pred.add_argument(
        "--next-observation",
        type=str,
        default=None,
        dest="next_observation",
        help="if set, target-encode and emit sigma_model + verdict vs z_predicted",
    )
    pred.add_argument("--imagine", action="store_true", help="latent roll-out along --actions up to --horizon")
    pred.add_argument("--plan", action="store_true", help="pick lowest final_sigma candidate from --candidates JSONL")
    pred.add_argument("--horizon", type=int, default=10, help="max steps for --imagine or --plan")
    pred.add_argument(
        "--actions",
        type=str,
        default="",
        help='comma-separated action strings for --imagine (default: repeat "noop")',
    )
    pred.add_argument(
        "--candidates",
        type=str,
        default=None,
        help="JSONL file: each object with \"actions\": [..] or \"plan\": [..] for --plan",
    )
    pred.add_argument("--k-raw", type=float, default=0.92, dest="k_raw", help="k_raw passed to sigma_update")
    pred.add_argument("--dim", type=int, default=8, help="lab latent dimension")
    pred.add_argument("--drift", type=float, default=0.02, help="LabLatentPredictor drift")
    pred.set_defaults(func=_cmd_predict)

    interp = sub.add_parser(
        "interpret",
        help="σ-SAE interpretability lab (v121): decompose, steer, correlate, … → JSON",
    )
    ig = interp.add_mutually_exclusive_group(required=True)
    ig.add_argument("--decompose", action="store_true", help="reconstruction + dominance + CB plan + L5")
    ig.add_argument("--steer", action="store_true", help="SSL-shaped direction steering + L5 on steered h")
    ig.add_argument("--steer-permanent", action="store_true", dest="steer_permanent", help="SALVE-shaped recurrence ledger")
    ig.add_argument("--feature-map", action="store_true", dest="feature_map", help="CB-SAE per-feature metrics")
    ig.add_argument("--explain", action="store_true", help="active feature labels + L5 attribution")
    ig.add_argument("--correlate", action="store_true", help="CorrSteer-shaped Pearson vs σ series (JSON inputs)")
    interp.add_argument("--mock", action="store_true", help="CI / no-torch stub JSON")
    interp.add_argument("--hidden-json", type=str, default="", dest="hidden_json", metavar="PATH", help="JSON array of floats")
    interp.add_argument("--dim", type=int, default=8, help="activation dim when --hidden-json omitted")
    interp.add_argument("--dict-dim", type=int, default=16, dest="dict_dim", help="SAE dictionary width (ToySAE)")
    interp.add_argument("--labels", type=str, default="", help='comma labels for --explain (default "f0",…) ')
    interp.add_argument("--halluc-features", type=str, default="", dest="halluc_features", help="comma feature indices")
    interp.add_argument("--activation-threshold", type=float, default=0.1, dest="activation_threshold")
    interp.add_argument("--tau-interp", type=float, default=0.2, dest="tau_interp")
    interp.add_argument("--tau-steer", type=float, default=0.2, dest="tau_steer")
    interp.add_argument("--clarify-tau", type=float, default=0.62, dest="clarify_tau")
    interp.add_argument("--faithful-json", type=str, default="", dest="faithful_json", help="JSON array, steering direction")
    interp.add_argument("--halluc-json", type=str, default="", dest="halluc_json", help="JSON array, direction to suppress")
    interp.add_argument("--scale-faithful", type=float, default=0.05, dest="scale_faithful")
    interp.add_argument("--scale-halluc-down", type=float, default=0.1, dest="scale_halluc_down")
    interp.add_argument("--feature-idx", type=int, default=0, dest="feature_idx")
    interp.add_argument("--replay", type=int, default=1, help="repeat record_hallucination_hit (steer-permanent)")
    interp.add_argument("--min-hits", type=int, default=3, dest="min_hits")
    interp.add_argument("--alpha-crit", type=float, default=0.15, dest="alpha_crit")
    interp.add_argument("--sigma-drop", type=float, default=0.25, dest="sigma_drop")
    interp.add_argument("--sigma-json", type=str, default="", dest="sigma_json", metavar="PATH")
    interp.add_argument("--activations-json", type=str, default="", dest="activations_json", metavar="PATH")
    interp.set_defaults(func=_cmd_interpret)

    bmk = sub.add_parser(
        "benchmark",
        help="host micro-bench (σ tiny, GEMM vs BSC, BitNet kernel lab; wraps Makefile targets)",
    )
    bmk.add_argument(
        "--dim",
        type=int,
        default=4096,
        metavar="N",
        help="vector dimension for BSC/GEMM paths (multiple of 64; default 4096)",
    )
    bmk.add_argument(
        "--hardware",
        action="store_true",
        help="run bench-hardware (sigma perf + GEMM vs BSC + Verilator lint when installed)",
    )
    bmk.add_argument(
        "--sigma-throughput",
        action="store_true",
        dest="sigma_throughput",
        help="run bench-sigma-perf (env: SIGMA_PERF_ITERS)",
    )
    bmk.add_argument(
        "--bsc-vs-gemm",
        action="store_true",
        dest="bsc_vs_gemm",
        help="run bench-gemm-bsc with CREATION_OS_GEMM_BSC_DIM from --dim",
    )
    bmk.add_argument(
        "--bsc-simd",
        action="store_true",
        dest="bsc_simd",
        help="alias of --bsc-vs-gemm (SIMD path is reported in the bench output)",
    )
    bmk.add_argument(
        "--energy-per-verdict",
        action="store_true",
        dest="energy_per_verdict",
        help="run bench-sigma-perf and note that joules/verdict needs external power counters",
    )
    bmk.add_argument(
        "--bitnet-sigma",
        action="store_true",
        dest="bitnet_sigma",
        help="run bench-bitnet-sigma (ternary matvec + sigma_gate_tiny + cache/spec toy)",
    )
    bmk.add_argument(
        "--bitnet-neon",
        action="store_true",
        dest="bitnet_neon",
        help="alias: same as --bitnet-sigma (see dispatch_speedup_over_scalar on AArch64)",
    )
    bmk.add_argument(
        "--cache-stats",
        action="store_true",
        dest="cache_stats",
        help="alias: same as --bitnet-sigma (see cache_hit_pct in output)",
    )
    bmk.add_argument(
        "--early-exit",
        action="store_true",
        dest="early_exit",
        help="alias: same as --bitnet-sigma (see early_exit_pct in output)",
    )
    bmk.add_argument(
        "--energy-per-token",
        action="store_true",
        dest="energy_per_token",
        help="run bench-bitnet-sigma; mj/token needs external power counters",
    )
    bmk.add_argument(
        "--bitnet-turbo",
        action="store_true",
        dest="bitnet_turbo",
        help="run bench-bitnet-turbo (kernel bench + sigma_gate_tiny throughput)",
    )
    bmk.add_argument(
        "--hybrid",
        action="store_true",
        help="run bench-hybrid (σ-sparse attention + SSM + hybrid lab; not llama.cpp)",
    )
    bmk.add_argument(
        "--seq-len",
        type=str,
        default="4096",
        dest="seq_len",
        metavar="LIST",
        help="comma-separated sequence lengths for --hybrid (default 4096)",
    )
    bmk.add_argument(
        "--ratio",
        action="store_true",
        dest="hybrid_ratio",
        help="with --hybrid: print SSM vs attention ratio lines only",
    )
    bmk.add_argument(
        "--memory",
        action="store_true",
        dest="hybrid_memory",
        help="with --hybrid: print memory story lines only",
    )
    bmk.add_argument(
        "--spike-vs-continuous",
        action="store_true",
        dest="spike_vs_continuous",
        help="emit JSON energy ratio lab model (Python only — no Makefile)",
    )
    bmk.set_defaults(func=_cmd_benchmark)

    spk = sub.add_parser(
        "spike",
        help="σ-spike / LIF lab: token sparsity, Ω lanes, energy JSON (see tests/test_sigma_spike.py)",
    )
    spk.add_argument("--prompt", type=str, default="", dest="spike_prompt", help="whitespace token drive for SigmaSpike")
    spk.add_argument("--threshold", type=float, default=0.05, dest="spike_threshold")
    spk.add_argument("--omega", action="store_true", dest="spike_omega", help="14-lane OmegaSpikeLab step")
    spk.add_argument("--turns", type=int, default=1, dest="spike_turns")
    spk.add_argument("--input", type=str, default=None, dest="spike_input", help="prompt text for LIF drive (v154)")
    spk.add_argument("--layers", type=int, default=14, dest="spike_layers")
    spk.add_argument("--spread", type=float, default=1.0, dest="spike_spread")
    spk.add_argument("--energy", action="store_true", dest="spike_energy", help="σ-spike network energy_report JSON")
    spk.add_argument("--benchmark", action="store_true", dest="spike_benchmark", help="wall-clock sparse vs dense LIF")
    spk.add_argument("--n", type=int, default=1000, dest="spike_benchmark_n", help="iterations for --benchmark")
    spk.set_defaults(func=_cmd_spike)

    tin = sub.add_parser("tiny", help="v160 σ-tiny footprint + sensor σ demo JSON")
    tin.add_argument("--footprint", action="store_true", dest="tiny_footprint")
    tin.add_argument("--sensor", action="store_true", dest="tiny_sensor")
    tin.add_argument("--baseline", type=int, default=0, dest="tiny_baseline")
    tin.add_argument("--tolerance", type=int, default=1, dest="tiny_tolerance")
    tin.add_argument("--reading", type=int, default=0, dest="tiny_reading")
    tin.set_defaults(func=_cmd_tiny)

    thk = sub.add_parser(
        "think",
        help="Full cognitive Fabric pipeline (default); use --jepa for v123 σ-JEPA JSON lab",
    )
    thk.add_argument(
        "think_input",
        nargs="?",
        default="",
        help="Question or goal for the full σ-Fabric stack (perceive→…→gate→observe)",
    )
    thk.add_argument(
        "--jepa",
        action="store_true",
        dest="think_jepa_lab",
        help="Run legacy v123 plan_argmin / visualize mode (--prompt required)",
    )
    thk.add_argument("--prompt", type=str, dest="think_prompt", default="", help="With --jepa: planning prompt")
    thk.add_argument("--horizon", type=int, default=5, dest="think_horizon")
    thk.add_argument("--planning-steps", type=int, default=100, dest="think_planning_steps")
    thk.add_argument("--visualize", action="store_true", dest="think_visualize")
    thk.add_argument("--actions", type=str, default="", dest="think_actions")
    thk.add_argument(
        "--trace",
        action="store_true",
        dest="think_trace",
        help="Print per-layer trace rows after the Fabric step",
    )
    thk.set_defaults(func=_cmd_think)

    spl = sub.add_parser("split", help="σ-split routing + layer_split lab JSON (v135)")
    spl.add_argument("--prompt", type=str, default="", dest="split_prompt")
    spl.add_argument("--privacy", type=str, default="normal", dest="split_privacy")
    spl.add_argument("--layer", action="store_true", dest="split_layer")
    spl.add_argument("--split-point", type=int, default=8, dest="split_point")
    spl.set_defaults(func=_cmd_split)

    flt = sub.add_parser("fleet", help="σ-fleet device registry JSON (v135 lab)")
    flt.add_argument("--register", action="store_true", dest="fleet_register")
    flt.add_argument("--health", action="store_true", dest="fleet_health")
    flt.add_argument("--device", type=str, default="device", dest="fleet_device")
    flt.add_argument("--model", type=str, default="unknown", dest="fleet_model")
    flt.add_argument("--state", type=str, default="", dest="fleet_state")
    flt.set_defaults(func=_cmd_fleet)

    att = sub.add_parser("attention", help="σ-attention full vs pruned token counts (JSON)")
    att.add_argument("--input", type=str, required=True, dest="attn_input")
    att.add_argument("--window", type=int, default=4, dest="attn_window")
    att.add_argument("--threshold", type=float, default=0.3, dest="attn_threshold")
    att.set_defaults(func=_cmd_attention)

    engram = sub.add_parser(
        "engram",
        help="Engram v2 store/recall with --state-file JSON (legacy; prefer `cos memory`)",
    )
    engram.add_argument("--state-file", type=str, required=True, dest="memory_state")
    engram.add_argument("--store", type=str, default=None, dest="memory_store")
    engram.add_argument("--recall", type=str, default=None, dest="memory_recall")
    engram.add_argument("--sigma", type=float, default=0.05, dest="memory_sigma")
    engram.add_argument("--verdict", type=str, default="ACCEPT", dest="memory_verdict")
    engram.add_argument("--tau", type=float, default=0.3, dest="memory_tau")
    engram.set_defaults(func=_cmd_memory)

    bn = sub.add_parser("bitnet", help="σ-BitNet packed ternary lab JSON (v158)")
    bn.add_argument("--sparsity", action="store_true", dest="bitnet_sparsity")
    bn.add_argument("--prompt", type=str, default="", dest="bitnet_prompt")
    bn.add_argument("--dim", type=int, default=8, dest="bitnet_dim")
    bn.add_argument("--layers", type=int, default=4, dest="bitnet_layers")
    bn.add_argument("--k-raw", type=float, default=0.92, dest="bitnet_k_raw")
    bn.set_defaults(func=_cmd_bitnet)

    twn = sub.add_parser("twin", help="σ-twin lab workspace JSON (v162)")
    twn.add_argument("--workspace", type=str, required=True, dest="twin_workspace")
    twn.add_argument("--create", action="store_true", dest="twin_create")
    twn.add_argument("--experiment", action="store_true", dest="twin_experiment")
    twn.add_argument("--name", type=str, default="exp", dest="twin_exp_name")
    twn.add_argument("--change", type=str, default="", dest="twin_change")
    twn.set_defaults(func=_cmd_twin)

    jp = sub.add_parser("jepa", help="σ-JEPA world_model_predict JSON (lab; alias of deeper predict stack)")
    jp.add_argument("--predict", action="store_true", dest="jepa_predict")
    jp.add_argument("--observation", type=str, default="", dest="jepa_observation")
    jp.add_argument("--action", type=str, default="", dest="jepa_action")
    jp.set_defaults(func=_cmd_jepa_cli)

    moe = sub.add_parser("moe", help="σ-MoE expert registry JSON (v146 lab)")
    moe.add_argument("--register", type=str, default="", dest="moe_register_name", metavar="EXPERT")
    moe.add_argument("--model", type=str, default="", dest="moe_register_model")
    moe.add_argument("--registry", type=str, default="", dest="moe_registry", metavar="PATH")
    moe.set_defaults(func=_cmd_moe_cli)

    zkp = sub.add_parser("zkp", help="σ-ZKP lab: prove_verdict / verify_proof JSON (hash commitments)")
    zkp.add_argument("--prove", action="store_true", dest="zkp_prove")
    zkp.add_argument("--verify", action="store_true", dest="zkp_verify")
    zkp.add_argument("--prompt", type=str, default="", dest="zkp_prompt")
    zkp.add_argument("--response", type=str, default="", dest="zkp_response")
    zkp.add_argument("--model-anchor", type=str, default="", dest="zkp_model_anchor")
    zkp.add_argument("--proof", type=str, default="", dest="zkp_proof_path", metavar="PATH")
    zkp.add_argument("--expected-commitment", type=str, default="", dest="zkp_expected_commitment")
    zkp.set_defaults(func=_cmd_zkp_cli)

    fs = sub.add_parser(
        "fewshot",
        help="v183 σ-fewshot: prototypical embeddings + ICL + σ transfer check (lab)",
    )
    fs.add_argument("--learn", action="store_true", dest="fewshot_learn")
    fs.add_argument("--predict", action="store_true", dest="fewshot_predict")
    fs.add_argument("--adapt", action="store_true", dest="fewshot_adapt")
    fs.add_argument("--clear", action="store_true", dest="fewshot_clear")
    fs.add_argument("--task", type=str, default="", dest="fewshot_task")
    fs.add_argument("--examples", type=str, default="", dest="fewshot_examples", metavar="JSON")
    fs.add_argument("--query", type=str, default="", dest="fewshot_query")
    fs.add_argument("--example", type=str, default="", dest="fewshot_example", metavar="JSON")
    fs.add_argument("--state", type=str, default="", dest="fewshot_state", metavar="PATH")
    fs.set_defaults(func=_cmd_fewshot)

    sy = sub.add_parser(
        "symbolic",
        help="v184 σ-symbolic: backward chaining + unification + resolution (lab)",
    )
    sy.add_argument("--assert", type=str, default="", dest="symbolic_assert", metavar="ATOM")
    sy.add_argument("--rule", type=str, default="", dest="symbolic_rule", metavar="RULE")
    sy.add_argument("--query", type=str, default="", dest="symbolic_query", metavar="ATOM")
    sy.add_argument("--clear", action="store_true", dest="symbolic_clear")
    sy.add_argument("--state", type=str, default="", dest="symbolic_state", metavar="PATH")
    sy.add_argument("--resolve-demo", action="store_true", dest="symbolic_resolve_demo")
    sy.add_argument("--clause-a", type=str, default="", dest="symbolic_clause_a", metavar="JSON")
    sy.add_argument("--clause-b", type=str, default="", dest="symbolic_clause_b", metavar="JSON")
    sy.set_defaults(func=_cmd_symbolic)

    sil = sub.add_parser(
        "silicon",
        help="v159 σ-silicon lab: benchmark JSON + semantic sigma_update smoke (sigma_gate_core reference)",
    )
    sil.add_argument("--benchmark", action="store_true", dest="silicon_benchmark")
    sil.add_argument("--simulate", action="store_true", dest="silicon_simulate")
    sil.add_argument("--test", type=str, default="", dest="silicon_test", metavar="SPEC")
    sil.set_defaults(func=_cmd_silicon)

    srv = sub.add_parser(
        "serve",
        help="σ-gate HTTP API: FastAPI REST + WebSocket + SSE (pip install 'creation-os[serve]'; default 0.0.0.0:8000)",
    )
    srv.add_argument("--host", type=str, default="0.0.0.0", dest="serve_host")
    srv.add_argument("--port", type=int, default=8000, dest="serve_port")
    srv.set_defaults(func=_cmd_serve)

    ag = sub.add_parser("agent", help="σ-gated agent runtime (mock or harness wiring)")
    ag.add_argument("--goal", type=str, required=True)
    ag.add_argument(
        "--firewall",
        action="store_true",
        help="run σ-firewall input precheck on --goal before the agent loop (lab encoder)",
    )
    ag.add_argument("--max-steps", type=int, default=10, dest="max_steps")
    ag.add_argument("--sandbox", action="store_true", help="reserved: tighter execution envelope")
    ag.add_argument(
        "--allow-destructive",
        action="store_true",
        help="allow destructive tools to pass σ policy (still σ-gated)",
    )
    ag.add_argument("--audit-dir", type=str, default="~/.cos/audit", help="where to append JSONL audit rows")
    ag.add_argument("--mock", action="store_true", help="built-in mock planner + gate for CI smoke")
    ag.add_argument("--json", action="store_true", help="print full step list as JSON")
    ag.set_defaults(func=_cmd_agent)

    auton = sub.add_parser(
        "autonomous",
        help="σ-gated autonomous loop until converge or halt (lab; not cos agent SigmaAgent)",
    )
    auton.add_argument("autonomous_goal", type=str, metavar="GOAL", help="task goal string")
    auton.add_argument("--max-steps", type=int, default=20, dest="autonomous_max_steps")
    auton.add_argument("--timeout", type=int, default=300, dest="autonomous_timeout")
    auton.add_argument("--drift-threshold", type=float, default=0.3, dest="drift_threshold")
    auton.set_defaults(func=_cmd_autonomous)

    gen = sub.add_parser(
        "genesis",
        help="Six-stage cognitive primitive smoke (boot→persist; NOT AGI ACHIEVED — see docs/CLAIM_DISCIPLINE.md)",
    )
    gen.add_argument(
        "--engram-path",
        type=str,
        default="",
        dest="genesis_engram",
        metavar="PATH",
        help="optional Engram JSON path (default: ~/.cos/engram.json)",
    )
    gen.set_defaults(func=_cmd_genesis)

    rep = sub.add_parser(
        "repro",
        help="Build a claim-discipline repro bundle JSON (requires git checkout for SHA)",
    )
    rep.add_argument("--name", type=str, default="demo", dest="repro_name", metavar="NAME")
    rep.add_argument(
        "--output-dir",
        type=str,
        default="",
        dest="repro_output",
        metavar="DIR",
        help="override eval_results/<name> parent",
    )
    rep.add_argument("--json", action="store_true", dest="repro_json", help="print validation + path as JSON")
    rep.set_defaults(func=_cmd_repro)

    ns = ap.parse_args(argv)
    fn = getattr(ns, "func", None)
    if fn is None:
        ap.print_help()
        return 2
    return int(fn(ns))


if __name__ == "__main__":
    raise SystemExit(main())
