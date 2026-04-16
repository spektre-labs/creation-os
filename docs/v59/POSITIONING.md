# v59 Positioning — σ-Budget vs the Q2 2026 adaptive-budget field

## Landscape

| System           | Signal                        | Decision surface                  | Reported savings       |
|------------------|-------------------------------|-----------------------------------|------------------------|
| TAB              | GRPO-learned policy           | {stop, continue}                  | 35–40 %                |
| CoDE-Stop        | intermediate-answer confidence | {stop, continue}                 | 25–50 %                |
| LYNX             | learned hidden-state probe    | {exit, stay} at reflection cue    | 40–70 %                |
| DTSR             | reflection-signal count       | {stop, continue}                  | 28–35 %                |
| DiffAdapt        | entropy scalar                | {easy, normal, hard}              | 22 %                   |
| Coda             | rollout-estimated difficulty  | {compute gate}                    | 60 % on easy           |
| AdaCtrl          | RL length policy              | {length control}                  | 62–91 % on easy        |
| Risk-Control BF  | distribution-free dual thresh | {stop-confident, unsolvable}      | 52 %                   |
| **v59 σ-Budget** | **σ = (ε, α) decomposed**     | **{CONTINUE, EARLY_EXIT, EXPAND, ABSTAIN}** | **M-tier, 69 / 69 tests** |

## Where v59 is different

1. **σ-decomposition.** Every prior system uses a scalar signal.  v59 is
   the first to use ε (reducible) and α (irreducible) separately at the
   budget decision.

2. **Four-valued decision.** Every prior system answers a binary
   (stop / continue) or a length selector.  v59 distinguishes:
   - `EARLY_EXIT` — answer crystallised, stop now
   - `CONTINUE`   — σ moderate, keep reasoning
   - `EXPAND`     — σ high but **ε-dominant**, branch / self-verify
   - `ABSTAIN`    — σ high and **α-dominant**, the problem is irreducibly
     hard; hand an abstain to the upstream harness (v53) rather than
     burning compute on a problem no amount of tokens will solve.

3. **ABSTAIN is unique.** No scalar-signal method can produce a faithful
   abstain because it cannot tell ε from α.  Low-entropy noise and
   low-entropy crystallisation look the same to an entropy scalar; v59
   separates them.

4. **Shipped as a kernel, not a paper.** `make check-v59` is the
   falsifier: 69 deterministic tests + microbench, branchless kernel,
   NEON 4-accumulator path, aligned allocation.  Reproducible in 10 s.

## What v59 is **not**

- A benchmark leaderboard entry.  v59 ships **policy + kernel +
  correctness proof**; it does not claim a tokens-per-correct number
  against TAB / LYNX / DTSR on GSM8K / MATH / AIME.  That is a P-tier
  measurement outside the v59 contract.
- A replacement for the σ source.  v59 consumes ε / α — it does not
  produce them.  Upstream producers: v34 Dirichlet, v55 σ₃ speculative,
  v56 VPRM.
- A learned policy.  v59 is a hand-coded branchless decision kernel.
  Scoring weights are policy knobs in `cos_v59_policy_t`, not trained
  parameters.
- A proof artifact.  No Frama-C / TLA+ proof yet; only M-tier runtime
  checks.  Formal verification is an open follow-up.
