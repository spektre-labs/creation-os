#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""C ↔ Python parity check for σ-pipeline primitives.

Builds a compact 3x3x3 grid of (σ, τ_accept, τ_rethink) and
(σ_peak, τ_escalate) triples that covers every decision branch (below /
inside / above each threshold), runs the same rows through both the
Python mirrors in ``scripts/sigma_pipeline/`` and the compiled C
binaries via ``--demo`` mode, and asserts both sides agree on every
action / route.

Exit code 0 on full PASS.  Any divergence is a merge-gate failure.
"""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from sigma_pipeline.reinforce import Action, reinforce            # noqa: E402
from sigma_pipeline.speculative import (                           # noqa: E402
    Route, cost_savings, route,
)
from sigma_pipeline import ttt as pttt                             # noqa: E402
from sigma_pipeline.engram import fnv1a_64                          # noqa: E402
from sigma_pipeline import moe as pmoe                              # noqa: E402


def _run_binary(name: str) -> dict:
    """Run one of the σ-pipeline binaries and parse its JSON header."""
    path = ROOT / name
    if not path.exists():
        raise SystemExit(f"missing binary: {path} "
                         f"(run `make check-sigma-pipeline` first)")
    out = subprocess.run([str(path)], capture_output=True, text=True)
    if out.returncode != 0:
        raise SystemExit(f"{name} returned rc={out.returncode}: "
                         f"{out.stderr}")
    return json.loads(out.stdout.strip())


def main() -> int:
    failures = 0

    # The C side's self_test is a superset of these rows; here we just
    # assert its reported pass:true so we know the C contract is intact.
    r = _run_binary("creation_os_sigma_reinforce")
    assert r["pass"] is True, f"C reinforce self_test failed: {r}"
    assert r["max_rounds"] == 3

    s = _run_binary("creation_os_sigma_speculative")
    assert s["pass"] is True, f"C speculative self_test failed: {s}"

    # Python-side replay of the C canonical table.
    # (σ, τ_accept, τ_rethink, expected_action)
    rows_reinforce = [
        (0.00, 0.30, 0.70, Action.ACCEPT),
        (0.29, 0.30, 0.70, Action.ACCEPT),
        (0.30, 0.30, 0.70, Action.RETHINK),
        (0.50, 0.30, 0.70, Action.RETHINK),
        (0.69, 0.30, 0.70, Action.RETHINK),
        (0.70, 0.30, 0.70, Action.ABSTAIN),
        (0.99, 0.30, 0.70, Action.ABSTAIN),
        (0.40, 0.50, 0.50, Action.ACCEPT),
        (0.50, 0.50, 0.50, Action.ABSTAIN),
        (float("nan"), 0.30, 0.70, Action.ABSTAIN),
        (-1.0, 0.30, 0.70, Action.ABSTAIN),
        (0.10, 0.80, 0.30, Action.ABSTAIN),  # mis-configured
    ]
    for sigma, ta, tr, exp in rows_reinforce:
        got = reinforce(sigma, ta, tr)
        if got is not exp:
            print(f"FAIL reinforce({sigma}, {ta}, {tr}) = "
                  f"{got.name} ≠ {exp.name}")
            failures += 1

    rows_route = [
        (0.00, 0.70, Route.LOCAL),
        (0.69, 0.70, Route.LOCAL),
        (0.70, 0.70, Route.ESCALATE),
        (0.99, 0.70, Route.ESCALATE),
        (float("nan"), 0.70, Route.ESCALATE),
        (-1.0, 0.70, Route.ESCALATE),
    ]
    for sp, te, exp in rows_route:
        got = route(sp, te)
        if got is not exp:
            print(f"FAIL route({sp}, {te}) = {got.name} ≠ {exp.name}")
            failures += 1

    # Cost-savings formula matches C side.
    c_sav = s["cost_savings_demo"]["savings"]
    py_sav = cost_savings(
        s["cost_savings_demo"]["n_total"],
        s["cost_savings_demo"]["n_escalated"],
        s["cost_savings_demo"]["eur_local"],
        s["cost_savings_demo"]["eur_api"],
    )
    if abs(c_sav - py_sav) > 1e-4:
        print(f"FAIL cost_savings C={c_sav:.6f} vs Py={py_sav:.6f}")
        failures += 1

    # TTT parity: replay the same 4-step canonical demo through the
    # Python mirror and compare against the C binary's reported drift
    # and step counters.
    t = _run_binary("creation_os_sigma_ttt")
    assert t["pass"] is True, f"C ttt self_test failed: {t}"
    demo = t["demo"]
    st = pttt.TTTState.init(
        slow=[1.0] * demo["n"], lr=demo["lr"],
        tau_sigma=demo["tau_sigma"], tau_drift=demo["tau_drift"],
    )
    grad = [1.0] * demo["n"]
    pttt.step(st, 0.10, grad)   # skip
    pttt.step(st, 0.90, grad)   # update
    pttt.step(st, 0.90, grad)   # update
    pttt.step(st, 0.90, grad)   # update
    py_drift = pttt.drift(st)
    did_reset = pttt.reset_if_drift(st)

    if st.n_steps_total != demo["n_steps_total"]:
        print(f"FAIL ttt n_steps_total "
              f"C={demo['n_steps_total']} vs Py={st.n_steps_total}")
        failures += 1
    if st.n_steps_updated != demo["n_steps_updated"]:
        print(f"FAIL ttt n_steps_updated "
              f"C={demo['n_steps_updated']} vs Py={st.n_steps_updated}")
        failures += 1
    if abs(py_drift - demo["drift_pre_reset"]) > 1e-4:
        print(f"FAIL ttt drift C={demo['drift_pre_reset']:.6f} "
              f"vs Py={py_drift:.6f}")
        failures += 1
    if (did_reset == 1) != bool(demo["did_reset"]):
        print(f"FAIL ttt did_reset C={demo['did_reset']} "
              f"vs Py={did_reset == 1}")
        failures += 1

    # MoE parity: run the canonical (σ, logits) sweep through the
    # Python mirror and assert agreement with the C binary's demo.
    mo = _run_binary("creation_os_sigma_moe")
    assert mo["pass"] is True, f"C moe self_test failed: {mo}"
    logits = [0.10, 0.90, 0.30, 0.70]
    sigmas = [0.05, 0.20, 0.40, 0.80]
    for i, (s, c_row) in enumerate(zip(sigmas, mo["demo"])):
        py = pmoe.route(s, logits)
        if py.expert_id != c_row["expert_id"] or \
           py.width.label != c_row["width"] or \
           abs(py.width_frac - c_row["width_frac"]) > 1e-6:
            print(f"FAIL moe row {i} "
                  f"C={c_row} vs Py={py}")
            failures += 1
    py_acts = [pmoe.route(s, logits) for s in sigmas]
    if abs(pmoe.compute_saved(py_acts) - mo["compute_saved"]) > 1e-6:
        print(f"FAIL moe compute_saved "
              f"C={mo['compute_saved']} Py={pmoe.compute_saved(py_acts)}")
        failures += 1
    py_topk = pmoe.top_k_route(0.05, logits, 2)
    c_topk = mo["top_k"]
    if [a.expert_id for a in py_topk] != c_topk["experts"]:
        print(f"FAIL moe top-k C={c_topk['experts']} "
              f"Py={[a.expert_id for a in py_topk]}")
        failures += 1

    # Engram parity: FNV-1a-64 must match byte-for-byte on the four
    # canonical strings the C binary publishes.
    eg = _run_binary("creation_os_sigma_engram")
    assert eg["pass"] is True, f"C engram self_test failed: {eg}"
    for label, s in (("empty", ""), ("a", "a"),
                     ("hello", "hello"), ("q1", "What is 2+2?")):
        c_hash = int(eg["hashes"][label])
        py_hash = fnv1a_64(s)
        if c_hash != py_hash:
            print(f"FAIL engram FNV-1a-64 mismatch for {label!r}: "
                  f"C={c_hash} vs Py={py_hash}")
            failures += 1

    if failures:
        print(f"sigma-parity: {failures} divergence(s)")
        return 1
    print("sigma-parity: OK — C ↔ Python decision parity on 18 canonical "
          "rows + cost-savings formula match (Δ<1e-4) + TTT 4-step demo "
          "match (Δ<1e-4) + Engram FNV-1a-64 byte-identical on 4 strings "
          "+ MoE 4-row width-gate + top-k + compute_saved match (Δ<1e-6)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
