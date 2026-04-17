# v69 σ-Constellation — Architecture

## Why v69

v60..v68 close the **single-node** loop: security, lattice, reasoning,
cipher, MCTS, hypercortex, matrix, deliberation, and memory.  What is
still missing is **scale** — coordination across many inference paths,
many experts, many agents, many nodes.

Every 2026 frontier system has scale; none has a formally composed,
integer, branchless coordination floor.  `v69` ships it.

## Ten plus one

```
┌─────────────────────────────────────────────────────────────────────┐
│                     v69 σ-Constellation kernel                      │
├─────────────────────────────────────────────────────────────────────┤
│  1  tree speculative decoding (EAGLE-3 lineage)                     │
│  2  multi-agent debate (Council Mode + FREE-MAD)                    │
│  3  Byzantine 2f+1 quorum (PBFT-integer)                            │
│  4  MoE top-K routing (MaxScore)                                    │
│  5  draft tree expansion / prune                                    │
│  6  gossip / Lamport vector clocks                                  │
│  7  chunked flash-style attention (softmax-free integer)            │
│  8  Elo + UCB self-play arm selection                               │
│  9  KV-cache dedup via 512-bit bipolar popcount sketch              │
│ ──────────────────────────────────────────────────────────────────  │
│ 10  CL — Constellation Language (10-opcode integer bytecode ISA)    │
└─────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
                    cos_v69_compose_decision
                    (10-way branchless AND over v60..v69)
```

## Hardware discipline (.cursorrules invariants)

* **Every arena `aligned_alloc(64, …)`** — the draft tree, the vote
  grid, the dedup sketch table, the Elo rating table, and the CL
  interpreter state are 64-byte aligned at construction.  Hot paths
  (verify, vote, route, dedup) never allocate.
* **Q0.15 integer on every decision surface** — no FP anywhere on the
  GATE path.  No softmax.  No division on the hot path.
* **Branchless on the data** — tree verify, top-K route, Byzantine
  vote, vector-clock merge, dedup collapse are all branchless `sel_i32`
  over the payload.  Not constant-time in the fleet size (by design —
  scaling with N is the whole point).
* **`__builtin_popcountll` everywhere** — Byzantine vote counts,
  Hamming dedup distances, chunked dot score tracking all use native
  `cnt + addv` on AArch64.
* **No dependencies beyond libc** — `stdint.h`, `stdlib.h`,
  `string.h`, `time.h`.

## The CL bytecode

```
+-----+----+----+----+-------+-------+
| op  | dst| a  | b  |  imm  |  pad  |
| u8  | u8 | u8 | u8 |  i16  |  u16  |   ← 8 bytes per instruction
+-----+----+----+----+-------+-------+

ops : HALT(1)  DRAFT(4)  VERIFY(8)  DEBATE(16)  VOTE(4)
      ROUTE(4) GOSSIP(2) ELO(1)     DEDUP(2)    GATE(1)

GATE : v69_ok = (cost ≤ cost_budget) &
                (reg_q15[a] ≥ imm)   &
                (byz_fail  ≤ byzantine_budget) &
                (abstained == 0)
```

The cost integer is per-opcode, declared in the header, summed in the
interpreter, and compared against `cost_budget` with a single `<=`.
If the program exceeds budget, `abstained` flips and `GATE` cannot
succeed — the kernel refuses without branching on the payload.

## The 10-way compose

```c
d.allow = v60_ok & v61_ok & v62_ok & v63_ok & v64_ok
        & v65_ok & v66_ok & v67_ok & v68_ok & v69_ok;
```

One instruction.  Zero branches.  Full truth table covered by the
self-test (`test_compose_truth_table` iterates all 2¹⁰ = 1024
configurations).

## Data flow (a single orchestration step)

```
            ┌──────────────────────────────┐
            │   incoming tool invocation   │
            └──────────────┬───────────────┘
                           │
      ┌────────────────────┼────────────────────────┐
      │ v60..v67 (unchanged single-node path)       │
      └────────────────────┬────────────────────────┘
                           │
                           ▼
                ┌──────────────────────┐
                │   v68 σ-Mnemos       │
                │ (recall, ratchet)    │
                └──────────┬───────────┘
                           │
                           ▼
                ┌──────────────────────┐
                │   v69 σ-Constellation│
                │ DRAFT  → VERIFY      │
                │ DEBATE → VOTE        │
                │ ROUTE  → GOSSIP      │
                │ ELO    → DEDUP       │
                │ GATE                 │
                └──────────┬───────────┘
                           │
                           ▼
                cos_v69_compose_decision
                           │
                           ▼
                   allow ∈ {0, 1}
```

Each cardinal step is an **integer compare**; each transition is a
**single bit**; the final decision is a **single AND**.  No floating
point, no softmax, no stochastic gate anywhere on the decision path.

## Performance (Apple M-series performance core)

| Microbench                               | Throughput              |
|------------------------------------------|-------------------------|
| `tree_verify` depth=8                    | ~58 M verifies/s        |
| `debate_score` N=32                      | ~9.3 M aggs/s           |
| `byzantine_vote` N=64                    | ~129 M votes/s          |
| `route_topk` N=64 K=8                    | ~2.1 M routes/s         |
| `dedup_insert` Hamming<64, table=64      | ~6.9 M inserts/s        |
| `chunked_dot` N=1024 chunk=64            | ~7.4 M dots/s           |
| `elo_update + ucb_select` 16 arms        | ~69 M updates/s         |
| `cl_exec` 9-instruction end-to-end       | ~9.9 M progs/s (~89 M ops/s) |
| `compose_decision` 10-bit                | ~361 M decisions/s      |

All numbers reproducible via `make microbench-v69`.

## Verification

* **Self-tests**: 3226 deterministic assertions including the full
  2¹⁰-entry composed-decision truth table, fanout tree verify,
  Byzantine vote boundary, anti-conformity penalty invariant, vector-
  clock causality, Elo monotonicity, dedup collapse, and a full
  end-to-end 9-instruction CL program.
* **Sanitizer matrix**: `make asan-v69 ubsan-v69` — clean.
* **Hardening**: `make standalone-v69-hardened` — stack protector,
  PIE, _FORTIFY_SOURCE=2, RELRO, full-stack canary — clean.
* **Verify-agent**: `bash scripts/v57/verify_agent.sh` — 18 PASS,
  3 SKIP, 0 FAIL (v69 added as `distributed_orchestration` slot).

## One invariant

**1 = 1.**
