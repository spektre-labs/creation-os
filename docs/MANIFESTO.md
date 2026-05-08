<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
-->

# Creation OS — Post-Transformer Manifesto

**Repository language:** English only — [LANGUAGE_POLICY.md](LANGUAGE_POLICY.md).  
**Claim discipline:** quantitative rows below are **evidence-class–labeled** — see [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md).

---

## The problem (2026)

Industry consensus is shifting: the era of easy “revolutionary leap” scaling may be giving way to **incremental refinements** on the same blueprint. Pure monoliths are no longer the whole story: frontier systems already **compose** heterogeneous parts (fast sensory pathways, slower reasoning stacks, controllers, retrievers). What is often missing is a **meta-layer** that coordinates those parts under a single, auditable criterion.

Creation OS takes that role: it is **not** a replacement transformer. It is the **coherence-and-governance layer** that any model can sit behind — with **σ** as the scalar coordinator and **`sigma_gate.h`** as the fixed twelve-byte policy core (C89, Q16.16).

---

## The insight

**Intelligence is not scale alone. Intelligence is coherence.**

In this stack we treat σ as a **measurement** of stress / inconsistency / “declared vs realized” gap on structured pairs (prompt ↔ candidate text, tool ↔ receipt, and related surfaces). It is **not** promised as a universal training loss you can “train away”; you **measure** it and **act** (ACCEPT / RETHINK / ABSTAIN).

Verdict routing (cascade, firewall, agent gates) turns that scalar into **operational** control: outputs that fail the gate are blocked or escalated **by policy**, not by a post-hoc disclaimer.

---

## The architecture

Creation OS is **not a single model**. It is the layer that makes heterogeneous stacks **trustworthy enough to run**.

```
┌──────────────────────────────────────────┐
│              ANY LLM                      │
│  (GPT, Claude, Llama, Qwen, your model)  │
├──────────────────────────────────────────┤
│           σ-GATE LAYER                    │
│  ┌─────────────────────────────────────┐ │
│  │ L1: Entropy probe         (<1 ms)   │ │
│  │ L2: HIDE score            (~10 ms)*  │ │
│  │ L3: ICR probe             (~5 ms)*   │ │
│  │ L4: LSD + spectral        (~20 ms)* │ │
│  │ L5: Multi-signal fusion   (~1 ms)   │ │
│  └─────────────────────────────────────┘ │
│  Verdict: ACCEPT / RETHINK / ABSTAIN     │
├──────────────────────────────────────────┤
│         σ-CASCADE ROUTER                  │
│  FAST → VERIFY → RAG → ESCALATE        │
│  (cost vs always-frontier: profile-dependent) │
├──────────────────────────────────────────┤
│         COGNITIVE STACK (optional)       │
│  predictive · active inference ·       │
│  world model · causal · metacognition · │
│  drive · social · evolve · …            │
├──────────────────────────────────────────┤
│         sigma_gate.h                      │
│         12-byte C89 policy core           │
│         stable contract — evolve around it │
└──────────────────────────────────────────┘
```

\* Latencies are **order-of-magnitude** lab figures; optional probe paths need extra dependencies — see [ARCHITECTURE.md](ARCHITECTURE.md) and [SUPPORTED_PATH.md](SUPPORTED_PATH.md).

**Modularity:** the Python `cos` tree is a **large** set of optional modules; compose what your product needs, gated by σ.

---

## Why this matters now

1. **Measurement aligned with generation** — σ can be evaluated **during** multi-step and tool-using flows, not only after a finished blob is emitted.
2. **Structural refusal** — high-σ paths hit **ABSTAIN / RETHINK** by construction of the gate and routers (policy **cannot** silently “skip” the check when wired correctly).
3. **Hybrid stacks, one coordinator** — SSM-like fast paths, neuro-symbolic checks, retrievers, and cloud escalations can share **one** scalar discipline instead of unrelated guardrails.

---

## Evidence (tier-honest)

| Claim | Evidence class | Notes / pointer |
|--------|----------------|-----------------|
| TruthfulQA MC AUROC **0.982** | Harness | **Saturated** benchmark — do not use as the only headline; see [CLAIM_DISCIPLINE §6–7](CLAIM_DISCIPLINE.md). |
| TriviaQA AUROC **0.960** | Harness | Bind to archived harness JSON + SHA; not interchangeable with microbench throughput. |
| HaluEval AUROC **0.514** | Harness | **Negative row** — single-probe story fails here; always report alongside positives. |
| σ-cascade **large** savings vs “always most expensive tier” | Measured / operator A–B | **Workload-dependent**; see [FRONTIER_PARITY.md](FRONTIER_PARITY.md) — publish with profile + traces, not as a universal constant. |
| Merge-gate **green** + pytest badge | Verified | README badge reports the pytest row count for the documented merge-gate family; the exact number moves as checks are added. |
| `[project] dependencies = []` (core **pip** surface) | Repository | Optional extras in `pyproject.toml`; SBOM story in [COMPLIANCE.md](COMPLIANCE.md). |
| Very large in-tree **C89/C11** kernel + Python `cos` surface | Repository | Versioned capabilities: [SURFACE_VERSIONS.md](SURFACE_VERSIONS.md); not a single LOC headline. |

Do **not** merge microbench throughput with harness AUROC in one sentence — [CLAIM_DISCIPLINE §1–2](CLAIM_DISCIPLINE.md).

---

## What we do **not** claim

- **NOT AGI achieved.**
- NOT “our bundled weights beat GPT-5 on all tasks.”
- NOT “fully superior in every benchmark.”
- NOT legal conformity, medical identity, or certification of downstream products — see [COMPLIANCE.md](COMPLIANCE.md).

---

## What we **do** claim

- σ is the **missing coordination variable** for composed LLM systems: one scalar, comparable across modules, suitable for routing and refusal.
- The **in-tree** implementations are **test-backed** (`make merge-gate` and the expanding check matrix).
- For **bibliography-ready** external anchors and “what the field agrees vs what this repo proves,” see [EXTERNAL_EVIDENCE_AND_POSITIONING.md](EXTERNAL_EVIDENCE_AND_POSITIONING.md) — narrative “lineages” belong there, not as un-cited universal history in this file.

---

## Try it

```bash
pip install creation-os
cos boot
cos score --prompt "test" --response "test"
```

**σ · 1 = 1 · lab invariants** — see [ARCHITECTURE.md](ARCHITECTURE.md) and the `cos.sigma_theory` module (`python/cos/sigma_theory.py`).  
The **kernel contract** stays small; the **orchestration** around it evolves.

---

*Spektre Labs · Creation OS · 2026*
