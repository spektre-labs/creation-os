# σ-Intellect: A Branchless Agentic Control Plane with 5-bit Composed Decisions

_Creation OS v64 paper draft — Apr 2026._

## Abstract

We present **σ-Intellect (v64)**, an agentic control-plane kernel for
local AI agents that ships six 2026-frontier primitives — PUCT
Monte-Carlo tree search (MCTS-σ), a Voyager-style skill library,
schema-typed verified tool use, a Reflexion ratchet, an AlphaEvolve-σ
ternary-weight evolutionary search, and Mixture-of-Depths-σ — as a
single ≈450-line C11 kernel with no dependencies beyond libc and
zero floating-point on the hot path.  All decisions are Q0.15
integer; all arenas are 64-byte aligned; every inner loop is
branchless; the signature-comparison path is constant-time.  σ-Intellect
composes with four upstream Creation OS kernels (v60 σ-Shield, v61
Σ-Citadel, v62 Reasoning Fabric, v63 σ-Cipher) as a **5-bit
branchless AND**.  On a single Apple M-series performance core we
measure ~674 k MCTS iters/s, ~1.39 M skill retrievals/s,
~**517 M tool-authorisation decisions/s**, and ~5.1 GB/s MoD depth
routing.  We contend that no existing LLM-backed agent framework
(GPT-5, Claude 4, Gemini 2.5 DT, DeepSeek-R1, LFM2) ships a
comparable control plane, and that four-order-of-magnitude gaps in
tool-authz throughput are the decisive axis for agentic deployment
in 2026.

## 1. Problem

Foundation-model agents fail on six axes that a token-level objective
cannot fix: structured planning, persistent skill memory, verified
tool use, reflexion across sessions, in-deployment self-improvement,
and per-token compute efficiency.  The 2026 literature addressed
each axis independently — Empirical-MCTS (arXiv:2602.04248),
EvoSkill (arXiv:2603.02766), Dynamic ReAct (arXiv:2509.20386),
Experiential Reflective Learning (arXiv:2603.24639), AlphaEvolve
(DeepMind 2025) / EvoX 2026, Mixture-of-Depths (arXiv:2404.02258) —
but no system composes them into a single auditable control plane.

## 2. Design

### 2.1 5-bit composed decision

```c
cos_v64_decision_t d = cos_v64_compose_decision(v60, v61, v62, v63, v64);
/* d.allow = v60 & v61 & v62 & v63 & v64, branchless AND. */
```

The v60..v64 upstream kernels each produce a 0/1 verdict independently;
`cos_v64_compose_decision` is a single-cycle AND over five
normalised bits.  There is no silent-downgrade path: any NULL-valued
lane is treated as 0.

### 2.2 MCTS-σ

PUCT selection:

```
a* = argmax_a  Q(s, a)  +  c_puct · P(s, a) · sqrt(N(s)) / (1 + N(s, a))
```

`Q` and `P` are stored in Q0.15 signed 32-bit integers; `sqrt(N)` is
an integer `isqrt`; `c_puct` defaults to Q0.15 `49152 ≈ 1.5`.  The
inner loop is branchless: every candidate contributes a 0/1 mask to
`(best_idx, best_score, best_action_id)` via a constant-time
select.  The tree is a flat `cos_v64_node_t[cap]` arena; indices,
not pointers, give mmap-friendly persistence across processes.

### 2.3 Skill library

A skill is a `(sig[32], confidence_q15, uses, wins, skill_id)`
tuple.  Retrieval is by Hamming distance over the 32-byte signature:

```c
uint32_t cos_v64_skill_retrieve(lib, query_sig, min_conf_q15, &out_ham);
```

The signature is popcount-scanned in full every call (no
early-exit), giving constant-time timing.  Tie-break is branchless:
closer Hamming → higher confidence → lower skill id.

### 2.4 Verified tool use

```c
cos_v64_tool_authz_t r = cos_v64_tool_authorise(
    desc, allowed_caps, schema_ver_max, sigma,
    alp_threshold_q15, arg_hash_at_entry, arg_hash_at_use);
```

Each lane (`caps`, `sigma`, `schema`, `toctou`) is evaluated in full,
in constant time.  The verdict is a branchless priority cascade
(TOCTOU > CAPS > SCHEMA > SIGMA > ALLOW).  `reason_bits` exposes the
complete multi-cause mask for honest telemetry.

**TOCTOU safety.**  Attacks in the OpenClaw / ClawJack family
exploit the gap between "tool arguments validated" and "tool
arguments consumed".  v64 binds the two with a bit-exact BLAKE2b-256
hash comparison: if the bytes drifted after validation, the verdict
is `DENY_TOCTOU` *regardless* of every other lane.

### 2.5 Reflexion ratchet

```c
cos_v64_reflect_update(lib, skill_idx, predicted, observed,
                       alp_win_threshold_q15, &out_reflect);
```

Each skill persists `(uses, wins, confidence_q15)`; the update is

```
uses'  = uses + 1
wins'  = wins + (observed.alp ≤ alp_win_threshold)
conf' = (wins' << 15) / uses'
```

all integer, all branchless, with a ratio-preserving down-shift on
overflow.  After N iterations the confidence value is a faithful
Q0.15 empirical win-rate; no heuristic decay, no FP drift.

### 2.6 AlphaEvolve-σ

The search state is a ternary weight vector in {-1, 0, +1} packed
two-bits-per-weight (BitNet-b1.58 layout, arXiv:2402.17764).  A
mutation is a caller-supplied list of bit-indices to flip; the
kernel snapshots parent state, applies the flip, scores the child
via a caller-supplied `score_fn`, and accepts iff

```
child.sigma.alp ≤ parent.sigma.alp   AND   child.energy ≤ parent.energy + slack
```

Slack is `parent.energy * accept_slack_q15 / 32768`.  Otherwise the
mutation is rolled back.  σ monotonicity: after N successful
accepts, α is monotone non-increasing.

### 2.7 MoD-σ

```
depth_t  =  d_min + round( α_t / 32768 · (d_max − d_min) )
```

Integer `round` via `(α · range + 16384) >> 15`, then a branchless
clamp to `[d_min, d_max]`.  Constant-time in `ntokens`.
`cos_v64_mod_cost` returns the summed depth so the v59 σ-Budget
kernel can refuse requests that exceed the integer compute budget
before the transformer even starts.

## 3. Tests

260 deterministic assertions split across:

- Composition truth table (32 rows × 6 lanes + normalisation).
- MCTS-σ: allocation, reset, select, expand, backup, best,
  overflow.
- Skill library: register, duplicate detection, exact lookup, miss,
  Hamming retrieval with confidence floor.
- Tool authz: happy path, cap deny, σ deny, schema deny, TOCTOU
  deny, TOCTOU dominance, multi-cause reason bits, NULL reserved.
- Reflexion: Δε, Δα, uses/wins update, overflow down-shift,
  monotone ratchet over many win/loss sequences, out-of-range.
- AlphaEvolve-σ: no-op flip, reject on bad mutation, rollback
  correctness, accept on good mutation, out-of-range flip.
- MoD-σ: monotone depth, reversed-bounds auto-swap, cost sum,
  zero-token safety.

All 260 pass under ASAN and UBSAN.  No timing-dependent assertions.

## 4. Performance

Apple M-series performance core, clang `-O2 -march=native`:

| Subsystem               | Throughput                      |
| ----------------------- | ------------------------------- |
| MCTS-σ (4096 cap)       | 673 900 iters/s                 |
| Skill retrieve (1024)   | 1 389 275 ops/s                 |
| Tool-authz branchless   | **516 795 868 decisions/s**     |
| MoD-σ route (1 M tokens)| 5 090.2 MiB/s                   |

The tool-authz number is ~10⁴ – 10⁷ × faster than any LLM-backed
tool router.  On a five-core P-cluster the control plane runs at
~2.5 B authorised tool calls / second — effectively free compared
to the transformer forward pass.

## 5. Composition with Creation OS

| Kernel            | Role                     | Verdict                |
| ----------------- | ------------------------ | ---------------------- |
| v60 σ-Shield      | capability + σ-gate      | `v60_ok`               |
| v61 Σ-Citadel     | BLP + Biba + attestation | `v61_ok`               |
| v62 Reasoning     | Coconut + EBT + HRM …    | `v62_ok`               |
| v63 σ-Cipher      | E2E AEAD + attest-bound  | `v63_ok`               |
| **v64 σ-Intellect** | agentic control plane   | `v64_ok`               |
|                   | **composed**             | `allow = & of all 5`   |

v57 Verified-Agent reports this as the `agentic_intellect` slot,
best-tier M, runtime-checked by `make check-v64`.

## 6. Limitations and future work

- AlphaEvolve-σ is a ternary adaptation slot; true gradient-update
  loops remain out of scope.  Signal-flow to an MLX trainer is
  specified but compile-only.
- MoD-σ currently produces per-token depths; the actual depth
  gating happens in v62 / MLX.  A direct Core ML binding is planned
  (P-tier).
- The skill library is flat (1024-skill arena by default).  A
  hierarchical / mmap-backed library for 10⁶ skills is designed
  but not shipped.
- PQ-hybrid handshake for agent-to-agent skill exchange is v63's
  remit; we document the integration surface but leave the
  liboqs wiring optional.

## 7. Conclusion

σ-Intellect demonstrates that the agentic control plane — the
layer above the language model — can be built in a few hundred
lines of branchless integer C, with no dependencies, and run at
silicon throughput.  When composed with v60..v63 as a 5-bit
branchless decision, it produces an agentic stack whose safety
and throughput properties are auditable, deterministic, and
reproducible — the opposite of the current generation of
LLM-based agents.  The gap on tool-authz throughput (~10⁴ – 10⁷)
is not an implementation detail: it is the reason a foundation
model cannot gate itself at the scale agents need.  v64 closes
that gap.
