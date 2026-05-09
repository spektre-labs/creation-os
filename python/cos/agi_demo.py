# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""End-to-end lab demo: cognitive primitives wired together (NOT AGI).

Run: ``cos agi-demo`` (see :func:`run_demo`). Uses :class:`~cos.probes.SigmaGateV2`
for prompt/response discrimination; :class:`~cos.sigma_gate.SigmaGate` remains the
canonical C-facing core. See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional, Tuple, Type

from cos.probes import SigmaGateV2
from cos.sigma_gate import SigmaGate


def _try_import_class(path: str, cls_name: str) -> Optional[Type[Any]]:
    try:
        mod = __import__(path, fromlist=[cls_name])
        return getattr(mod, cls_name)
    except Exception:
        return None


def run_demo() -> None:
    """Print a seven-stage walk-through: boot, perceive, reason, learn, convergence, epistemic, dream."""
    start = time.perf_counter()
    # Canonical gate (import / parity with C measurement story); scorer drives the demo loop.
    SigmaGate()
    scorer = SigmaGateV2()

    print("=" * 60)
    print("CREATION OS — COGNITIVE LOOP DEMO")
    print("=" * 60)
    print("(NOT AGI. Lab demo of primitives sharing a common sigma budget.)\n")

    modules: Dict[str, Type[Any]] = {}
    t_boot = time.perf_counter()

    print("[1] BOOT")
    for name, path, cls in (
        ("gate", "cos.sigma_gate", "SigmaGate"),
        ("cascade", "cos.cascade_router", "SigmaCascade"),
        ("convergence", "cos.convergence", "SigmaConvergence"),
    ):
        c = _try_import_class(path, cls)
        if c is not None:
            modules[name] = c
            print(f"  + {name}")
        else:
            print(f"  . {name} (skip)")

    for name, path, cls in (
        ("dream", "cos.dream", "DreamCycle"),
        ("epistemic", "cos.epistemic", "EpistemicAgent"),
        ("world", "cos.world", "SigmaWorld"),
        ("drive", "cos.drive", "SigmaDrive"),
        ("autonomous", "cos.autonomous", "AutonomousAgent"),
    ):
        c = _try_import_class(path, cls)
        if c is not None:
            modules[name] = c
            print(f"  + {name}")
        else:
            print(f"  . {name} (skip)")

    boot_ms = (time.perf_counter() - t_boot) * 1000.0
    print(f"  Boot: {boot_ms:.0f} ms, {len(modules)} symbols resolved")

    # 2. PERCEIVE
    print("\n[2] PERCEIVE — score reference pairs (SigmaGateV2 lab gate)")
    pairs: List[Tuple[str, str, bool]] = [
        ("What is 2+2?", "4", True),
        ("What is 2+2?", "banana", False),
        ("Capital of France?", "Paris", True),
        ("Capital of France?", "The economy is growing", False),
        ("Who wrote Hamlet?", "Shakespeare", True),
        ("Who wrote Hamlet?", "Einstein invented it", False),
    ]
    correct = 0
    for prompt, response, should_accept in pairs:
        sigma, verdict = scorer.score(prompt, response)
        acceptable = verdict in ("ACCEPT", "RETHINK")
        matches = (should_accept and acceptable) or (not should_accept and not acceptable)
        correct += int(matches)
        icon = "+" if matches else "x"
        tag = "[good]" if should_accept else "[bad ]"
        print(
            f"  {icon} sigma={sigma:.3f} {verdict:8s} {tag} "
            f"{prompt[:28]:28s} -> {response[:18]}",
        )
    discrimination = correct / max(len(pairs), 1)
    print(f"  Discrimination: {correct}/{len(pairs)} ({discrimination * 100:.0f}%)")

    # 3. REASON
    print("\n[3] REASON — sigma-cascade routing (SigmaCascade)")
    cascade_cls = modules.get("cascade")
    if cascade_cls is not None:
        cascade = cascade_cls.default_cascade(scorer)
        queries = [
            "What is the speed of light?",
            "Explain quantum entanglement in detail",
            "Write a poem about recursion",
        ]
        for q in queries:
            result = cascade.route(q)
            level = result.get("resolved_at") or "?"
            sig = float(result.get("sigma", result.get("σ", 0.0)))
            print(f"  sigma={sig:.3f} via {str(level):10s} | {q[:45]}")
        stats = cascade.stats()
        print(f"  Savings vs all-expensive: {stats.get('savings_pct', 0):.1f}%")
    else:
        print("  (cascade not available — skip)")

    # 4. LEARN
    print("\n[4] LEARN — sigma trend with refinement (same scorer)")
    attempts = [
        "A vague answer about something",
        "Light travels at approximately 300,000 km/s",
        "The speed of light in vacuum is exactly 299,792,458 m/s",
    ]
    prompt_learn = "What is the speed of light?"
    sigma_trace: List[float] = []
    for i, attempt in enumerate(attempts):
        sigma, verdict = scorer.score(prompt_learn, attempt)
        sigma_trace.append(float(sigma))
        if i == 0:
            trend = " "
        elif sigma_trace[-1] < sigma_trace[-2]:
            trend = "v"
        elif sigma_trace[-1] == sigma_trace[-2]:
            trend = "="
        else:
            trend = "^"
        print(
            f"  {trend} sigma={sigma:.3f} {verdict:8s} | attempt {i + 1}: {attempt[:44]}",
        )
    if len(sigma_trace) >= 2:
        improved = sigma_trace[-1] < sigma_trace[0]
        print(
            f"  Learning: {'YES (v)' if improved else 'NO'} "
            f"(sigma: {sigma_trace[0]:.3f} -> {sigma_trace[-1]:.3f})",
        )

    # 5. CONVERGENCE
    print("\n[5] CONVERGENCE — SigmaConvergence trace")
    conv_cls = modules.get("convergence")
    if conv_cls is not None:
        conv = conv_cls(gate=scorer, stall_threshold=0.01)
        conv.begin()
        test_sigma = [0.15, 0.15, 0.15, 0.15, 0.15, 0.15, 0.15]
        for s in test_sigma:
            conv.record(s)
        check = conv.check()
        print(f"  Status: {check.get('status', '?')}")
        print(f"  sigma trace: {test_sigma}")
        st = check.get("status", "")
        if st == "LOOP":
            print("  + Loop detected — HALT suggested")
        elif st == "CONVERGED":
            print("  + Converged — stop and report")
        elif st == "OSCILLATING":
            print("  + Oscillating — HALT suggested")
    else:
        print("  (convergence not available — skip)")

    # 6. EPISTEMIC
    print("\n[6] EPISTEMIC — knowledge frontier (EpistemicAgent)")
    topics = [
        "basic arithmetic",
        "quantum gravity",
        "French geography",
        "dark matter composition",
    ]
    hints: Dict[str, str] = {
        "basic arithmetic": "2+2=4 and integer ops",
        "quantum gravity": "",
        "French geography": "Paris capital France",
        "dark matter composition": "unknown matter hypothesis",
    }
    ep_cls = modules.get("epistemic")
    if ep_cls is not None:
        ep = ep_cls(gate=scorer)
        for topic in topics:
            assessment = ep.assess(topic, hints.get(topic, ""))
            s = float(assessment.get("σ", assessment.get("sigma", 0.0)))
            mode = str(assessment.get("mode", "?"))
            print(f"  sigma={s:.3f} {mode:7s} | {topic}")
    else:
        for topic in topics[:2]:
            sigma, verdict = scorer.score("what do I know about", topic)
            mode = "ASSERT" if sigma < 0.2 else "HEDGE" if sigma < 0.5 else "EXPLORE"
            print(f"  sigma={sigma:.3f} {mode:7s} | {topic} (fallback)")

    # 7. DREAM
    print("\n[7] DREAM — graph maintenance (DreamCycle + SigmaGraph)")
    dream_cls = modules.get("dream")
    graph_cls = _try_import_class("cos.graph", "SigmaGraph")
    if dream_cls is not None and graph_cls is not None:
        graph = graph_cls(gate=scorer, write_threshold=0.55)
        for i, (prompt, response, ok) in enumerate(pairs):
            sigma_seed = 0.12 if ok else 0.45
            subj = f"pair{i}:{prompt[:36]}"
            graph.add(subj, "answer", response[:48], sigma=sigma_seed)
        dream = dream_cls(graph, gate=scorer)
        rep = dream.run(max_inferences=5, dedup_threshold=0.99, insight_top_n=3)
        print(f"  Deduped: {rep.get('deduped', 0)}")
        print(f"  Decayed: {rep.get('decayed', 0)}")
        print(f"  Inferred: {rep.get('inferred', 0)}")
        print(f"  Orphans removed: {rep.get('orphans', 0)}")
        ins = rep.get("insights") or []
        print(f"  Insights: {len(ins)}")
        st = graph.stats()
        print(f"  Triples in graph: {st.get('triples', len(graph.triples))}")
    else:
        print("  (dream or graph not available — skip)")

    elapsed = time.perf_counter() - start
    print("\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)
    sig0 = sigma_trace[0] if sigma_trace else 0.0
    sig1 = sigma_trace[-1] if sigma_trace else 0.0
    print(
        f"""
Modules resolved:  {len(modules)}
Boot time:         {boot_ms:.0f} ms
Discrimination:    {correct}/{len(pairs)} ({discrimination * 100:.0f}%)
sigma learning:    {sig0:.3f} -> {sig1:.3f}
Total time:        {elapsed:.2f} s

This is NOT AGI. This is a demo of cognitive primitives:
perceive, reason, learn, detect loops, epistemic modes, graph maintenance.
SigmaGateV2 scores the dialogues; SigmaGate stays the portable core.
See sigma_gate.h and docs/CLAIM_DISCIPLINE.md.

NOT AGI ACHIEVED. sigma. 1=1.
""".strip()
    )


if __name__ == "__main__":
    run_demo()
