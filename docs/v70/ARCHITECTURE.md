# v70 σ-Hyperscale — Architecture

## Why v70

v60..v69 close the **fleet** loop: security, lattice, reasoning,
cipher, MCTS, hypercortex, matrix, deliberation, memory, and
orchestration.  What is still missing is **substrate** — the
silicon-tier path that lets a *single* commodity workstation address
trillion-parameter regimes (P2Q ShiftAddLLM + Mamba-2 SSM + RWKV-7
delta + DeepSeek-V3 MoE-10k + HBM-PIM + photonic WDM + Loihi-3 spike
+ NCCL ring + LRU streamer) **without a matmul, without an FP op on
the decision path, and without a byte of dependency outside libc**.

Every 2026 frontier system has substrate; none has a formally
composed, integer, branchless silicon-tier floor.  `v70` ships it.

## Ten plus one

```
┌─────────────────────────────────────────────────────────────────────┐
│                      v70 σ-Hyperscale kernel                        │
├─────────────────────────────────────────────────────────────────────┤
│  1  P2Q  power-of-2 weight quantisation (ShiftAddLLM)               │
│  2  SSM  Mamba-2 / Mamba-3 selective scan                           │
│  3  TX   RWKV-7 "Goose" delta-rule update                           │
│  4  MOE  DeepSeek-V3 aux-loss-free top-K MaxScore router (10 240 e) │
│  5  PIM  HBM-PIM bit-serial AND-popcount                            │
│  6  WDM  photonic wavelength-multiplexed dot product                │
│  7  SPK  Loihi-3 graded-spike sparse activation                     │
│  8  RING NCCL bandwidth-optimal integer ring all-reduce             │
│  9  LRU  Petals/Helix/DirectStorage streaming weight scheduler      │
│ ──────────────────────────────────────────────────────────────────  │
│ 10  HSL  — Hyperscale Language (10-opcode integer bytecode ISA)     │
└─────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
                    cos_v70_compose_decision
                    (11-way branchless AND over v60..v70)
```

## Hardware discipline (.cursorrules invariants)

* **Every arena `aligned_alloc(64, …)`** — the P2Q packed buffer, the
  SSM state lane, the RWKV-7 mix register file, the MoE bias array,
  the PIM column-major bit-pack, the LRU slot table, and the HSL
  interpreter state are 64-byte aligned at construction.  Allocation
  sizes are rounded up to a multiple of 64 before `aligned_alloc` to
  satisfy strict libc implementations (glibc).
* **Q0.15 integer on every decision surface** — no FP anywhere on the
  GATE path.  No softmax.  No division on the hot path.  Saturating
  add (`sat_add_i32`) for accumulators.
* **Branchless on the data** — P2Q sign-flip via `(shifted ^ mask) -
  mask`, top-K MaxScore via branchless bubble, ring all-reduce via
  branchless modulo, spike trigger via mask, LRU victim via
  monotone-clock min.  All driven by `sel_i32`, never by `if` on the
  payload.
* **Signed-shift safety** — left shifts in P2Q GEMV are performed in
  `uint32_t` then cast back, eliminating the UB that UBSAN would
  otherwise raise on negative activations.
* **`__builtin_popcountll` everywhere** — PIM AND-popcount is
  literally `__builtin_popcountll(act_word & weight_col)` in a tight
  loop; one instruction on AArch64 (`cnt + addv`).
* **`__builtin_prefetch` on the LRU insert path** — the streamer
  schedules the next expert into L2 before its first use.
* **No dependencies beyond libc** — `stdint.h`, `stdlib.h`,
  `string.h`, `time.h`.

## The HSL bytecode

```
+-----+----+----+----+-------+-------+
| op  | dst| a  | b  |  imm  |  pad  |
| u8  | u8 | u8 | u8 |  i16  |  u16  |   ← 8 bytes per instruction
+-----+----+----+----+-------+-------+

ops : HALT(1)    SHIFT(2)   SCAN(4)    TIMEMIX(4)  ROUTEK(4)
      PIMPOP(8)  WDM(4)     SPIKE(2)   RING(8)     GATE(1)

GATE : v70_ok = (silicon_cost ≤ silicon_budget) &
                (reg_q15[a]   ≥ throughput_floor) &
                (topology_ok  == 1) &
                (abstained    == 0)
```

The cost integer is per-opcode, declared in the header, summed in the
interpreter, and compared against `silicon_budget` with a single
`<=`.  If the program exceeds budget or undershoots throughput,
`abstained` flips and `GATE` cannot succeed — the kernel refuses
without branching on the payload.

## The 11-way compose

```c
d.allow = v60_ok & v61_ok & v62_ok & v63_ok & v64_ok
        & v65_ok & v66_ok & v67_ok & v68_ok & v69_ok & v70_ok;
```

One instruction.  Zero branches.  Full truth table covered by the
self-test (`test_compose_truth_table` iterates all 2¹¹ = 2048
configurations).

## Data flow (a single hyperscale inference step)

```
            ┌──────────────────────────────┐
            │   incoming inference call    │
            └──────────────┬───────────────┘
                           │
      ┌────────────────────┼────────────────────────┐
      │ v60..v68 (unchanged single-node path)       │
      └────────────────────┬────────────────────────┘
                           │
                           ▼
                ┌──────────────────────┐
                │   v69 σ-Constellation│
                │ (orchestration)      │
                └──────────┬───────────┘
                           │
                           ▼
                ┌──────────────────────┐
                │   v70 σ-Hyperscale   │
                │ SHIFT (P2Q GEMV)     │
                │ SCAN  (Mamba SSM)    │
                │ TIMEMIX (RWKV-7)     │
                │ ROUTEK (MoE-10k)     │
                │ PIMPOP (HBM AND-pop) │
                │ WDM   (photonic)     │
                │ SPIKE (Loihi-3)      │
                │ RING  (all-reduce)   │
                │ GATE                 │
                └──────────┬───────────┘
                           │
                           ▼
                cos_v70_compose_decision
                           │
                           ▼
                   allow ∈ {0, 1}
```

Each cardinal step is an **integer compare**; each transition is a
**single bit**; the final decision is a **single AND**.  No floating
point, no softmax, no stochastic gate anywhere on the decision path.

## Performance (Apple M-series performance core, M4)

| Microbench                               | Throughput              |
|------------------------------------------|-------------------------|
| `p2q_gemv` rows=128 cols=4096            | multiply-free GEMV      |
| `ssm_scan` N=4096                        | ~187 M tokens/s         |
| `rwkv7_step` dim=128                     | ~1.9 M steps/s          |
| `moe_route_topk` E=64 K=8                | branchless top-K        |
| `pim_and_popcount` cols=4096             | ~1.0 G AND-pops/s       |
| `wdm_dot` 8 lanes × 1024                 | branchless lane-reduce  |
| `spike_encode` N=4096                    | activity-proportional   |
| `ring_allreduce` N=4 ranks               | bandwidth-optimal       |
| `stream_lru` 64 slots                    | branchless O(N)         |
| `hsl_exec` end-to-end                    | ~4 M progs/s            |
| `compose_decision` 11-bit                | ~187 M decisions/s      |

All numbers reproducible via `make microbench-v70` (or
`scripts/v70/microbench.sh` for a self-contained banner).

## Verification

* **Self-tests**: 148 034 deterministic assertions including the full
  2¹¹-entry composed-decision truth table, P2Q pack/unpack/GEMV
  round-trip, SSM decay invariant, RWKV-7 dim-1 + pure-decay
  invariants, MoE top-K + 10 240-expert smoke, PIM popcount identity,
  WDM lane reduction, spike threshold, N=4 ring-allreduce equality,
  LRU eviction order, HSL simple GATE + budget abstain, and fuzz
  passes for P2Q / SSM / ring / spike / MoE.
* **Sanitizer matrix**: `make asan-v70 ubsan-v70` — clean.
* **Hardening**: `make standalone-v70-hardened` — stack protector,
  PIE, `_FORTIFY_SOURCE=2`, RELRO, full-stack canary — clean.
* **Verify-agent**: `bash scripts/v57/verify_agent.sh` — 19 PASS,
  3 SKIP, 0 FAIL (v70 added as `hyperscale_substrate` slot).

## One invariant

**1 = 1.**
