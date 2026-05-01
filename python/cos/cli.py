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
from typing import Any, Dict, Iterator, List, Optional


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


def _cmd_omega(args: argparse.Namespace) -> int:
    from cos.omega import OmegaLoop

    loop = OmegaLoop()
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


def _cmd_observe(args: argparse.Namespace) -> int:
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
    if getattr(args, "no_warm", False):
        pass
    else:
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
        print(
            "cos mcp: use --list, --wrap --server CMD, and/or --register --server-id ID …",
            file=sys.stderr,
        )
        return 2
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
            pass
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


def main(argv: Optional[List[str]] = None) -> int:
    argv = argv if argv is not None else sys.argv[1:]
    ap = argparse.ArgumentParser(prog="cos", description="Creation OS cos CLI")
    sub = ap.add_subparsers(dest="cmd", required=True)

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
    om.add_argument("--goal", type=str, required=True, help="task / objective string")
    om.add_argument("--turns", type=int, default=50, help="maximum Ω turns (default 50)")
    om.add_argument("--mock", action="store_true", help="reserved for mock backends in harness wiring")
    om.add_argument("--json", action="store_true", help="print full turn history as JSON")
    om.set_defaults(func=_cmd_omega)

    mon = sub.add_parser("monitor", help="σ-native monitor (HTML dashboard; local-first)")
    mon.add_argument("--html", action="store_true", help="emit self-contained HTML (stdout unless --output)")
    mon.add_argument("--output", type=str, default="", help="write HTML to this path instead of stdout")
    mon.add_argument("--from-file", type=str, default="", dest="from_file", help="JSON array, JSONL, or object with steps/traces")
    mon.add_argument("--mock-drift", action="store_true", dest="mock_drift", help="built-in drifting σ fixture")
    mon.set_defaults(func=_cmd_monitor)

    ob = sub.add_parser("observe", help="σ-native observability (traces + alerts JSON)")
    ob.add_argument("--traces", action="store_true", help="include normalized σ-traces")
    ob.add_argument("--alerts", action="store_true", help="include σ-alert list")
    ob.add_argument("--from-file", type=str, default="", dest="from_file", help="JSON / JSONL same as cos monitor")
    ob.add_argument("--mock-drift", action="store_true", dest="mock_drift", help="synthetic drift for CI / demos")
    ob.set_defaults(func=_cmd_observe)

    fed = sub.add_parser(
        "federation",
        help="σ-federated lab (local train + optional stdlib HTTP; share σ-screened updates)",
    )
    fed.add_argument("--workspace", type=str, default="~/.cos/federation", help="state directory")
    fed.add_argument("--train", action="store_true", help="run local toy σ-FL rounds")
    fed.add_argument("--rounds", type=int, default=3, help="training rounds (with --train)")
    fed.add_argument("--no-poison", action="store_true", dest="no_poison", help="omit poison toy node")
    fed.add_argument("--no-byzantine", action="store_true", dest="no_byzantine", help="skip median outlier filter")
    fed.add_argument("--status", action="store_true", help="print fed_lab_state.json from workspace")
    fed.add_argument("--server", action="store_true", help="serve GET /status POST /train POST /join")
    fed.add_argument("--host", type=str, default="127.0.0.1", help="bind address (with --server)")
    fed.add_argument("--port", type=int, default=8080, help="TCP port (with --server)")
    fed.add_argument("--join", action="store_true", help="POST /join to --server-url")
    fed.add_argument("--server-url", type=str, default="", dest="server_url", help="e.g. http://127.0.0.1:8080")
    fed.add_argument("--data", type=str, default="", help="local data path label (with --join)")
    fed.set_defaults(func=_cmd_federation)

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

    mcp = sub.add_parser(
        "mcp",
        help="σ-MCP registry + middleware integration hints (Python lab; see also creation_os_sigma_mcp)",
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
    bmk.set_defaults(func=_cmd_benchmark)

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

    ns = ap.parse_args(argv)
    fn = getattr(ns, "func", None)
    if fn is None:
        ap.print_help()
        return 2
    return int(fn(ns))


if __name__ == "__main__":
    raise SystemExit(main())
