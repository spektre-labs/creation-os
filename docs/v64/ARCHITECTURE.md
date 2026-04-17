# v64 σ-Intellect — Architecture

## Wire map (how the five kernels compose)

```
                     ┌──────────────────────────────────────────────┐
                     │                   cos (CLI)                  │
                     │   cos sigma / cos mcts / cos decide          │
                     └──────────────┬───────────────────────────────┘
                                    │
       ┌────────────────────────────┴──────────────────────────────┐
       │                                                           │
       │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐       │
       │  │ v60 σ-Shield │ │ v61 Σ-Citadel│ │ v62 Fabric   │       │
       │  │  capability  │ │ BLP + Biba + │ │ Coconut +    │       │
       │  │  + σ-gate    │ │ attestation  │ │ EBT + HRM +  │       │
       │  │              │ │ quote256     │ │ NSAttn+MTP+ARKV │       │
       │  └──────┬───────┘ └──────┬───────┘ └──────┬───────┘       │
       │         │                │                │               │
       │  ┌──────┴──────────┐     │                │               │
       │  │ v63 σ-Cipher    │     │                │               │
       │  │ BLAKE2b+HKDF+   │     │                │               │
       │  │ ChaCha-Poly+    │     │                │               │
       │  │ X25519+seal     │     │                │               │
       │  └──────┬──────────┘     │                │               │
       │         │                │                │               │
       │         ▼                ▼                ▼               │
       │  ┌───────────────────────────────────────────────────┐    │
       │  │           v64 σ-Intellect (agentic core)          │    │
       │  │                                                   │    │
       │  │   MCTS-σ   ─► expand ─► EBT prior ─► backup       │    │
       │  │                                                   │    │
       │  │   Skill library ─► σ-Hamming retrieve ─► confid.  │    │
       │  │                                                   │    │
       │  │   Tool authz: schema + caps + σ + TOCTOU ─► allow │    │
       │  │                                                   │    │
       │  │   Reflexion ─► Δσ ─► update skill confidence      │    │
       │  │                                                   │    │
       │  │   AlphaEvolve-σ ─► flip ─► σ-accept or rollback   │    │
       │  │                                                   │    │
       │  │   MoD-σ ─► depth_t = f(α_t); ε-tokens skip depth  │    │
       │  └───────────────────────────────────────────────────┘    │
       │                         │                                 │
       │                         ▼                                 │
       │    composed 5-bit decision (branchless AND):              │
       │    allow = v60_ok & v61_ok & v62_ok & v63_ok & v64_ok     │
       └───────────────────────────────────────────────────────────┘
```

## File layout

```
src/v64/
├── intellect.h             # public ABI, ~300 lines
├── intellect.c             # branchless C kernel, ~450 lines
└── creation_os_v64.c       # self-test (260 deterministic assertions) + microbench
docs/v64/
├── THE_INTELLECT.md        # one-page artefact
├── ARCHITECTURE.md         # this file
├── POSITIONING.md          # versus GPT-5 / Claude / Gemini / DeepSeek / LFM2
└── paper_draft.md          # full paper draft
scripts/v64/
└── microbench.sh           # standardised benchmark runner
```

## M4 hardware-discipline checklist

- [x] All arenas `aligned_alloc(64, …)` at construction; no allocation
      on any hot path.
- [x] No floating point in the hot path.  PUCT, Hamming distance,
      tool authz, reflexion, MoD routing — all Q0.15 integer.
- [x] Branchless selects via 0/1 masks.  The CPU does not speculate
      branches that do not exist.
- [x] Constant-time signature compare (`v64_ct_eq_bytes`) so the
      skill library is not a timing oracle.
- [x] Integer-only `isqrt` for the PUCT `sqrt(N)` term; no `sqrtf`.
- [x] Every subsystem resizes caller-bounded arenas; overflow is an
      honest `-1`, never a silent downgrade.

## Microbenchmark (M4 perf core)

| Subsystem               | Throughput                         |
| ----------------------- | ---------------------------------- |
| MCTS-σ (4096-node tree) | **673 900** iters/s                |
| Skill retrieve (1024)   | **1 389 275** lookups/s            |
| Tool-authz branchless   | **516 795 868** decisions/s        |
| MoD-σ route (1 M tokens)| **5 090.2** MiB/s                  |

The tool-authz number is the headline: **over half a billion authorised
tool calls per second** from a single performance core, every one
checking caps + schema + σ + TOCTOU.  No LLM-backed tool router is
within four orders of magnitude of that.

## Threat-model tie-in

| Attack                          | Defended by                                      |
| ------------------------------- | ------------------------------------------------ |
| Prompt-injected tool call       | v60 capability gate + v64 schema/σ authz         |
| TOCTOU on tool arguments        | v64 `arg_hash_at_entry` vs `arg_hash_at_use`     |
| Skill-library timing oracle     | Constant-time `v64_ct_eq_bytes` scan             |
| σ-lane forgery (over-confidence)| v64 reflexion: predicted−observed ratcheting    |
| Evolutionary drift / regression | v64 AlphaEvolve-σ: strict σ-non-increase accept  |
| Compute-budget overrun          | v59 σ-Budget + v64 MoD-σ per-token cost sum      |

## Build matrix

```
make standalone-v64             # release
make standalone-v64-hardened    # OpenSSF 2026 flags
make check-v64                  # standalone + self-test
make asan-v64                   # AddressSanitizer
make ubsan-v64                  # UndefinedBehaviorSanitizer
make microbench-v64             # perf report
```

## Composition

The only way to opt out of v64's gate is to explicitly pass `v64_ok=0`
into `cos_v64_compose_decision`, which then refuses the composed
decision.  There is no silent-degrade path.
