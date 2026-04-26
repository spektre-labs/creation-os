# Neuro-symbolic boundary (informative)

Maintainer-facing note on how Creation OS names the interface between **probabilistic model output** (inference path) and **symbolic policy** (τ-gates, constitutional rules, receipts). This is not a benchmark report and not a substitute for [CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md). External research headlines belong in your own evidence bundle if you cite them.

## Conceptual stack

```text
┌──────────────────────────────────────────────┐
│         Symbolic (rules, gates, audit)      │
│  Constitution / Codex rules · τ-gate ·      │
│  proof receipts · optional formal tooling    │
├──────────────────────────────────────────────┤
│         Bridge (explicit summary)           │
│  cos_bridge_evaluate — digest + gate line   │
├──────────────────────────────────────────────┐
│         Neural (inference providers)         │
│  BitNet / HTTP / stubs — text + σ from path  │
└──────────────────────────────────────────────┘
```

## Code map

| Piece | Role |
|--------|------|
| `src/bridge/neural_symbolic.c` | `cos_bridge_evaluate`: takes prompt, response, combined σ, τ thresholds; runs pattern-layer Codex/constitution summary, τ-gate action, optional digest |
| `src/codex/codex_smt.c` | `cos_codex_smt_evaluate`: pattern-oriented checks only; **no Z3** in-tree — naming reflects “constraint-style” evaluation, not an SMT solver link |
| `cos chat --dual-process` | After the normal turn output and receipt, prints an **informative** System 1 / System 2 audit block (stderr under TUI so layout stays intact) |

The bridge **does not** re-run the full pipeline; it summarises a completed `(text, σ)` pair for transparency and tests.

## CLI

```bash
./cos chat --once --prompt "What is 2+2?" --dual-process --verbose
```

Use `--dual-process` together with your usual inference flags; behaviour without a live model still exercises the symbolic summary on whatever response the pipeline produced.

## Related docs

- [architecture/agi_stack.md](agi_stack.md) — broader layer map
- [CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md) — what may be stated vs what needs harness/archived evidence
