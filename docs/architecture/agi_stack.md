# Creation OS — AGI-oriented stack (informative map)

This document is a **maintainer-facing architecture sketch**. It is not a benchmark result, not a peer-reviewed AGI definition, and not a substitute for [CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md). External headlines (e.g. surveys of model behaviour) belong in your own evidence bundle if you cite them in prose.

## Layered view (conceptual)

```text
┌─────────────────────────────────────────────┐
│            CREATION OS (concept map)       │
├─────────────────────────────────────────────┤
│ L7  Self-model (lab)   │ predictive σ-world│
│ L6  World beliefs      │ KG world_model.c  │
│ L5  GVU / evolver      │ κ, τ adaptation   │
│ L4  Formal / assurance │ Lean, Frama-C, CI │
│ L3  Receipts / audit   │ SHA-256, JSONL    │
│ L2  σ-gate             │ pipeline + entropy│
│ L1  Codex              │ constitutional    │
│ L0  Inference          │ BitNet / HTTP     │
├─────────────────────────────────────────────┤
│ Memory / continual     │ engram, Ω-loop    │
└─────────────────────────────────────────────┘
```

## Predictive σ-world (lab)

**Code:** `src/omega/predictive_world.c`, `src/omega/continual_learning.c`

Coarse domain buckets, prompt-shape heuristics (length, negation hints, token-shape “entities”), and an exponential moving average of observed σ per domain. The Ω-loop calls `cos_predictive_world_omega_note_turn` after each turn so statistics accumulate during `cos omega` runs.

**CLI:** `cos self-report [--prompt TEXT] [--model ID]` prints the running snapshot. Precall estimates are **not** harness accuracy; bind any quality claim to `make bench` / archived harness JSON per claim discipline.

## Continual learning (this layer)

`cos_continual_learning_tick` applies a **slow statistics-only** cadence (decay on idle domains, widening a placeholder window field). It does **not** replace engram consolidation, pattern extraction, or disk persistence — those live in existing Ω / engram modules.

## Distinct modules

| Module | Role |
|--------|------|
| `src/sigma/world_model.c` | Knowledge-graph beliefs (`cos_world_*`) |
| `src/omega/predictive_world.c` | Running σ priors / EMA by coarse domain |

Do not conflate the two in documentation or metrics.
