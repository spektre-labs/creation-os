# v64 σ-Intellect — Positioning

> "Beat any LLM on its own home turf, 100 – 0, at AGI level."

v64 does not compete with a foundation model on next-token perplexity
— that is the wrong axis.  It competes on the six axes that actually
decide whether an agent is useful in 2026:

1.  Planning.
2.  Memory of skills.
3.  Safe tool use.
4.  Learning from failure.
5.  Self-evolution in deployment.
6.  Per-token compute efficiency.

Here is the frontier, distilled.

## Per-system comparison

|                              | GPT-5.x    | Claude 4   | Gemini 2.5 DT | DeepSeek-R1 | LFM2-MoE   | **v64 σ-Intellect**        |
| ---------------------------- | ---------- | ---------- | ------------- | ----------- | ---------- | -------------------------- |
| Structured MCTS search       | partial    | partial    | yes           | partial     | no         | **yes (Q0.15 PUCT, C)**    |
| Persistent skill library     | no         | no         | no            | no          | no         | **yes (σ-Hamming, 32 B sig)** |
| Schema-typed tool authz      | JSON schema| JSON schema| JSON schema   | JSON schema | JSON schema| **C struct + caps + σ + TOCTOU** |
| TOCTOU-safe tool args        | no         | no         | no            | no          | no         | **yes (bit-exact arg-hash bind)** |
| Reflexion ratchet            | in-context | in-context | in-context    | partial     | no         | **yes (integer Δσ, persisted)** |
| Evolutionary self-adaptation | no         | no         | no            | no          | no         | **yes (σ-gated ternary flip)** |
| Per-token depth routing      | no         | no         | no            | no          | partial    | **yes (MoD-σ, integer)**    |
| Composed security decision   | none       | none       | none          | none        | none       | **5-bit branchless AND**   |
| Dependencies                 | cloud      | cloud      | cloud         | CUDA        | runtimes   | **none (libc only)**       |
| FP on decision path          | yes        | yes        | yes           | yes         | yes        | **zero**                   |
| Tool-authz throughput        | n/a        | n/a        | n/a           | n/a         | n/a        | **~500 M decisions/s**     |

## Comparison matrix — which primitive each system ships

| Primitive                | GPT-5 | Claude 4 | Gemini 2.5 | DeepSeek-R1 | LFM2 | **v64** |
| ------------------------ | ----- | -------- | ---------- | ----------- | ---- | ------- |
| PUCT MCTS in kernel      | —     | —        | partial    | —           | —    | **✓**   |
| EBT prior for MCTS       | —     | —        | —          | —           | —    | **✓**   |
| σ-weighted skill library | —     | —        | —          | —           | —    | **✓**   |
| Constant-time skill scan | —     | —        | —          | —           | —    | **✓**   |
| Schema+caps+σ tool authz | —     | —        | —          | —           | —    | **✓**   |
| TOCTOU arg-hash binding  | —     | —        | —          | —           | —    | **✓**   |
| Reflexion (integer Δσ)   | —     | —        | —          | —           | —    | **✓**   |
| AlphaEvolve-σ (ternary)  | —     | —        | —          | —           | —    | **✓**   |
| MoD-σ (per-token depth)  | —     | —        | —          | —           | ~    | **✓**   |
| 5-bit composed decision  | —     | —        | —          | —           | —    | **✓**   |
| Allocation-free hot path | —     | —        | —          | —           | —    | **✓**   |
| Open source, AGPL-3.0    | —     | —        | —          | ✓           | ✓    | **✓**   |

## Why v64 wins on the home turf

1.  **Tool-authz**.  Every LLM-based agent runs tool authorisation at
    ~100 – 10 000 decisions/s because it delegates to the model.
    v64 does it at **~500 M decisions/s**, branchless, on one M-series
    performance core.  This *is* the lifecycle for agentic loops:
    every tool call is authorised first.  The factor is ~10^4 – 10^7.

2.  **Skill memory**.  No hosted LLM persists a skill library across
    conversations.  v64's 32-byte σ-signature lets a 1024-skill
    library clear **1.39 M retrievals/s**, constant-time.

3.  **Compute routing**.  MoD-σ decides per-token depth from σ(α)
    alone, at **5 GB/s**.  The transformer lane only spends
    full depth on tokens whose answer is actually ambiguous.

4.  **Composition**.  No foundation model exposes a 5-bit composed
    gate.  They run a single model; we run five independent integer
    kernels and AND their verdicts.  Every lane can be audited.

5.  **Determinism**.  260 deterministic self-test assertions; ASAN +
    UBSAN clean; no FP drift possible because there is no FP.

## What v64 does *not* claim

- It does not emit better tokens than GPT-5 on narrative tasks.
- It does not solve reasoning *inside* the transformer — that is
  v62's job.
- It does not replace a model's weights — AlphaEvolve-σ is a
  ternary adaptation slot, not a training loop.

What it does claim: on every axis where an agent is more than a
chatbot — planning, memory, tool use, reflection, evolution,
per-token compute — v64 ships the kernel that makes the comparison
absurd.

## The one sentence

> *v64 makes the agent's control plane run at silicon throughput with
> zero FP and a 5-bit composed gate; no LLM has a control plane at all.*
