# Hypervector parliament + NEON retrieval — AGI-shaped gate (hardware-first)

## What this is

Two **native** primitives in **4096-bit** space:

1. **`cos_hv_bit_parliament`** (`core/cos_parliament.h`) — **odd K** agents each hold one HV; output is **per-bit strict majority** across agents. For **K = 3**, this is **bit-for-bit identical** to `cos_hv_maj3` (proven in `tests/test_bsc_core.c`). For larger odd K, it is a **distributed vote** in HV space — not a frontier LLM, but a **deterministic fusion** you can run before expensive stacks.

2. **`cos_hv_argmin_hamming_rows_hw`** (`core/cos_neon_retrieval.h`) — scan a **row-major memory bank** (`n_rows × COS_W` words) with **NEON Hamming** on AArch64 (prefetch between rows). This is the “**which memory still matches my fused policy HV?**” gate in one tight loop.

## Benchmark

```bash
make bench-agi-gate
```

Default: **7 agents**, **2048** memory rows, **500** rounds of parliament + argmin (see `bench/hv_agi_gate_neon.c`). Override at compile time:

```bash
make bench-agi-gate CFLAGS='-O2 -march=native -Wall -std=c11 -DN_MEM=4096 -DN_ROUNDS=200'
```

Evidence class for wall-clock: **Measured (microbench)** — archive `uname`, compiler line, and full stdout per [BENCHMARK_PROTOCOL.md](BENCHMARK_PROTOCOL.md).

## Limits (read before tweeting)

- **Not** multi-agent RL convergence theory — it is **bit geometry + search**.  
- **Not** robot safety certification — still needs mechanics, torque limits, and your threat model ([SECURITY_DEVELOPER_NOTES.md](SECURITY_DEVELOPER_NOTES.md)).  
- Parliament requires **odd K**; even K is unsupported (silent no-op in current inline guard).

## Relation to industry framing

Cheap structural fuse + fast memory hit is the same **receipt-before-forward-pass** story as [COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md](COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md).

---

*Spektre Labs · Creation OS · 2026*
