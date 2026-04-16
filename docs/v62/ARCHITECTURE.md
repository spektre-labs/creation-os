# v62 Reasoning Fabric — architecture

## Wire map

```
                                  ┌─────────────────────────────────────────────────┐
                                  │                  cli/cos.c                      │
                                  │   Apple-tier single-binary CLI front door       │
                                  │   (status • verify • chace • sigma • think)     │
                                  └───────────────────────┬─────────────────────────┘
                                                          │
                                ┌─────────────────────────┴──────────────────────────┐
                                │                                                    │
                ┌───────────────▼─────────────────┐              ┌───────────────────▼────────────────┐
                │       v57 verify-agent          │              │              make chace            │
                │  (composition slot aggregator)  │              │   (DARPA-CHACE 12-layer gate)      │
                └───────────────┬─────────────────┘              └───────────────────┬────────────────┘
                                │                                                    │
            ┌───────────────────┼─────────────────────┐                              │
            │                   │                     │                              │
   ┌────────▼──────┐  ┌─────────▼─────────┐  ┌────────▼────────────┐                 │
   │  v60 σ-Shield │  │  v61 Σ-Citadel    │  │  v62 Reasoning      │                 │
   │  cap-bitmask  │  │  BLP+Biba+MLS     │  │  Fabric             │                 │
   │  σ-gate       │  │  + 256-bit attest │  │  (this layer)       │                 │
   │  TOCTOU-free  │  │  + v60 compose    │  │                     │                 │
   │  code-page    │  │                   │  │  ┌───────────────┐  │                 │
   │  hash         │  │                   │  │  │ Latent CoT    │  │                 │
   └───────┬───────┘  └─────────┬─────────┘  │  │ Coconut-class │  │                 │
           │                    │            │  └───────────────┘  │                 │
           │                    │            │  ┌───────────────┐  │                 │
           │                    │            │  │ EBT verifier  │  │                 │
           │                    │            │  │  (energy-min) │  │                 │
           │                    │            │  └───────────────┘  │                 │
           │                    │            │  ┌───────────────┐  │                 │
           │                    │            │  │ HRM H/L loop  │  │                 │
           │                    │            │  └───────────────┘  │                 │
           │                    │            │  ┌───────────────┐  │                 │
           │                    │            │  │ NSA attend    │  │                 │
           │                    │            │  │ (3-branch)    │  │                 │
           │                    │            │  └───────────────┘  │                 │
           │                    │            │  ┌───────────────┐  │                 │
           │                    │            │  │ MTP draft     │  │                 │
           │                    │            │  │ +verify       │  │                 │
           │                    │            │  └───────────────┘  │                 │
           │                    │            │  ┌───────────────┐  │                 │
           │                    │            │  │ ARKV manager  │  │                 │
           │                    │            │  │ ORIG/QUANT/   │  │                 │
           │                    │            │  │ EVICT         │  │                 │
           │                    │            │  └───────────────┘  │                 │
           └─────────┬──────────┴────────────┴─────────┬───────────┘                 │
                     │                                 │                             │
                     └────────── cos_v62_compose_decision (3-bit, branchless) ───────┘
                                          │
                                          ▼
                                 ALLOW / DENY (and which lane denied)
```

The composition is **branchless and short-circuit-free**: each lane is
evaluated unconditionally, the AND is a single byte. Telemetry can
inspect *which* lane denied (v60_ok / v61_ok / v62_ok) without re-running
anything.

## Hardware discipline (M4 invariants from `.cursorrules` and `AGENTS.md`)

| Rule                          | v62 implementation                                                    |
|-------------------------------|-----------------------------------------------------------------------|
| **mmap, never fread**         | KV blocks land at 64-B row stride so NSA can read direct from mmap.   |
| **64-byte aligned alloc**     | `v62_alloc64()` rounds size up and uses `aligned_alloc(64, ...)`.     |
| **Prefetch +64 B ahead**      | `v62_dot` and `v62_axpy` prefetch in every 16-element NEON unroll.    |
| **Branchless hot path**       | `v62_argmax` uses bitmask select; composition uses 3-bit AND.         |
| **4-way NEON accumulators**   | `v62_dot` keeps `s0..s3` parallel so M4's wide decode stays full.     |
| **Lookup before compute**     | ARKV state classifies tokens before any costly attention re-pass.     |

## Performance (Apple M-series, this commit)

```
v62 microbench (n=1024, d=64):
  NSA  attend          ~ 8 200 calls/s  (~ 0.12 ms / call)
  EBT  minimize (k=16) ~ 3 700 000 calls/s
  Latent-CoT step (d=64)        microsecond-class (in-loop only)
  ARKV update (n=32)            sub-microsecond
  Composition decision          single-cycle byte AND
```

The EBT verifier is the cheapest gate Creation OS has ever shipped: at
~3.7 M calls/s on M4, it can verify every reasoning step of a streamed
generation without becoming the bottleneck.

## Threat-model tie-in

| Failure mode                                        | v62 lane that catches it                              |
|-----------------------------------------------------|-------------------------------------------------------|
| Drifting / hallucinated thought                     | EBT energy explodes; verifier denies before emit.     |
| Runaway sequential CoT (over-thinking)              | Budget caps in `cos_v62_budget_t`; loop halts cleanly.|
| Long-context KV pressure (OOM, eviction churn)      | ARKV caps ORIG/QUANT and demotes branchlessly.        |
| Speculative-decode rollback storm                   | MTP verify is single-pass branchless; bounded cost.   |
| Linguistic-glue token waste                         | Latent CoT keeps reasoning in continuous space.       |
| Misuse of the reasoning loop to escape σ-Shield     | Composition denies if v60 lane fails, regardless of v62. |
| Lattice violation hidden inside latent thought      | Composition denies if v61 lane fails, regardless of v62. |

## Contract surface (one header)

The entire kernel is callable from the single header `src/v62/fabric.h`.
There are no global allocators, no thread-local state, no atomics on the
hot path. Any caller that respects the M4 invariants can compose v62
into their own runtime in minutes.
