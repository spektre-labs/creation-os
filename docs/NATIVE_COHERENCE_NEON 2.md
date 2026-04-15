# Native coherence path — NEON Hamming (`cos_neon_hamming.h`)

## Why this exists (industry gap)

Public surveys on **embodied foundation models at the edge** stress **SWaP**, **latency**, and **separating fast control from slow semantic reasoning**. A practical missing layer is a **cheap, deterministic** signal that two internal HV states still agree **before** another heavy forward pass or actuator command.

This header implements **4096-bit Hamming distance** (same geometry as `core/cos_bsc.h`) with:

- **AArch64:** NEON (`vcntq_u8` + `vaddlvq_u8` on 128-bit XOR lanes) plus explicit **prefetch** of the next word pair.
- **Other CPUs:** identical semantics via existing scalar `__builtin_popcountll` loop.

Evidence class for wall-clock: **Measured (microbench)** — archive host metadata per [BENCHMARK_PROTOCOL.md](BENCHMARK_PROTOCOL.md) when citing `hamming_checks/sec`.

## Build / run

```bash
make bench-coherence
```

## Tests

On AArch64, `tests/test_bsc_core.c` compares scalar vs hardware Hamming on the same vectors (must match bit-for-bit).

## Limits

- This is **distance only**, not a full robot safety stack — see [COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md](COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md) and [SECURITY_DEVELOPER_NOTES.md](SECURITY_DEVELOPER_NOTES.md).

## Next gate up-stack

Multi-agent **fusion + bank argmin** loop: [HYPERVECTOR_PARLIAMENT_AND_RETRIEVAL.md](HYPERVECTOR_PARLIAMENT_AND_RETRIEVAL.md) (`make bench-agi-gate`).

---

*Spektre Labs · Creation OS · 2026*
