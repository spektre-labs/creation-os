# σ-Guided Self-Play (v42 lab)

v42 is a **portable C scaffold** for a self-play loop where:

- a **challenger** proposes a short “problem” string whose difficulty is steered by epistemic σ (toy calibration),
- a **solver** proposes a solution string and receives a **σ + consistency** shaped reward (no external verifier in this lab path),
- an **experience buffer** supports prioritized batching for future training hooks,
- a **self-play driver** adjusts a difficulty target and includes a **limit-breaking** stub (external data request counter).

## Evidence class

- **Shipped today:** `make check-v42` internal consistency checks — **lab demo (C)**.
- **Not claimed without harness rows:** “data-free self-improvement beats GSM8K baselines” — **N** until `benchmarks/v42/*.json` exists with a REPRO bundle.

## Commands

```bash
make check-v42
make bench-v42-curve   # stub; exits 0 until RUN_V42_SELFPLAY_HARNESS=1 + evaluator exists
```

## Working paper title (only when a harness exists)

**“σ-Guided Self-Play: Uncertainty-Decomposed Reward Signals Enable Data-Free Self-Improvement of Language Models”**

Until then: treat σ-reward here as a **hypothesis interface** to be calibrated against external verifiers where stakes are high (math/code).
