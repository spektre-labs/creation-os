# σ-Constitutional: Branchless C11 Wiring of Rule-Based Process Verification, σ-Gated Test-Time Training, Grokking Commutator σ-Channel, and Apple Neural Engine Dispatch Layout

**Authors:** Elias Lorenzo (Spektre Labs, Helsinki) · ORCID 0009-0006-0903-8541
**License:** AGPL-3.0-or-later · **Tier:** scaffold (C) · **Hardware:** Apple M-series (aarch64) primary, POSIX scalar fallback

---

## Abstract

We present **σ-Constitutional**, a C11 scaffold that composes four
Q1-2026 research directions — rule-based process reward models
(VPRM, arXiv:2601.17223), in-place test-time training
(IP-TTT, arXiv:2604.06169), grokking as phase transition under
Singular Learning Theory (SLT, arXiv:2603.01192), and the Apple
Neural Engine's (ANE) undocumented `matmul → 1×1 conv`
requirement — into a single invariant:

> **Any inference-time self-modification must strictly lower σ.**

σ here is the same primitive Creation OS uses end-to-end
(`K_eff = (1 − σ) · K`, v40 threshold theorem).  v56 adds a σ-gated
budget controller for fast-weight updates, a commutator-defect
σ-channel orthogonal to output-distribution σ, a rule-based
verifier whose σ is `1 − precision`, and an honest layout helper
for ANE dispatch — all with NEON SIMD hot paths, no allocations,
no network, and a 56/56 deterministic self-test.

## Motivation

The 2026 research frontier has independently converged on three
hard questions.  **VPRM** argues that deterministic rule-based
verifiers beat neural process-reward models by up to 20 % F1, and
that precision is the priority metric because false positives build
an asymptotic ceiling.  **IP-TTT** demonstrates that MLP-projection
fast-weight updates at inference time let a 4 B model match much
larger static baselines on 128 K-context tasks, but does not
specify when a runtime should allow such an update.  **SLT
grokking** shows that the commutator defect between consecutive
weight updates spikes 10–1000× 600–1600 steps before
generalization — a phase transition visible in the trajectory
before it is visible in the loss.

At the same time, 2026 reverse-engineering of Apple's Neural
Engine has uncovered a shape contract no public Core ML
documentation surfaces: the MIL `matmul` op **silently does not
execute on ANE**, and every matmul must be rewritten as a 1 × 1
convolution with spatial dims ≥ 16 and multiples of 16.

Each of these findings is shippable in isolation.  Together they
describe a coherent inference-side discipline that Creation OS's
σ primitive can govern: **trust the weights while σ is low; when
σ drifts up, consider a fast-weight update — but only if the
commutator channel says the trajectory is stationary, only within a
bounded buffer, only if a rule-based verifier says the candidate
reasoning chain satisfies its process contract, and only if the
post-update σ does not regress**.  Dispatch that update's GEMM
through an ANE-valid layout or fall back to NEON.

## Contribution

1. **Rule-based σ-verifier** (`verifier.{h,c}`).  VPRM anchored;
   seven deterministic rules (nonempty, length bounds, score
   bounds, score monotonicity, no exact repeat, char-entropy);
   `sigma_verifier = 1 − precision`.  Precision-first aggregation
   matches the VPRM paper's priority metric.
2. **σ-gated IP-TTT budget controller** (`ipttt.{h,c}`).  Slow /
   recent EWMA bands on σ; allow iff byte budget, cooldown, and σ
   drift threshold are all met; commit iff post-update σ does not
   regress, else rollback with `alpha_current *= rollback_shrink`.
   The inference-time dual of `K_eff = (1 − σ) · K`.
3. **Grokking commutator σ-channel** (`grokking.{h,c}`).
   Normalized defect `‖Δg‖² / (‖g_new‖² + ‖g_prev‖²)`, bounded in
   `[0, 1]`, NEON 4-accumulator hot path with prefetch on both
   inputs; baseline-EWMA spike detection with warm-up.  First
   public runtime exposure of the SLT trajectory signal in an
   inference loop.
4. **Apple Neural Engine dispatch layout helper**
   (`ane_layout.{h,c}`).  `matmul → 1×1 conv` shape rewrite,
   spatial ≥ 16 & multiple-of-16 validator, padding plan, reason
   strings.  Pure integer math; honest scaffold under the ANE
   binding everyone wants but no one can ship portably.
5. **One invariant, four modules, 56 deterministic tests**.  Full
   self-test suite passes on aarch64 (NEON) and scalar fallbacks;
   integration test wires all four modules in the σ-Constitutional
   loop and verifies they compose.

## Key results (self-test, not benchmarks)

| module | tests | outcome |
| --- | --- | --- |
| verifier         | 5  | 5 / 5 pass (precision, F1, σ tracking, entropy) |
| ipttt            | 6  | 6 / 6 pass (cooldown, no-drift, drift, budget, rollback, commit) |
| grokking         | 5  | 5 / 5 pass (identical, opposite, orthogonal, NEON/scalar parity, spike) |
| ane_layout       | 5  | 5 / 5 pass (round-up, compatibility, NCHW shapes, reason) |
| integration      | 6  | 6 / 6 pass (clean chain → no TTT update; stable traj → no phase transition; ANE plan) |

These numbers certify the **math and policy** v56 ships, not TTT
throughput or ANE kilotokens-per-second on a real model.  Those
are explicitly out of scope; they belong in a live lab on top of
this scaffold.

## What we deliberately did not do

- We did **not** train a transformer to grok, measure the
  commutator defect, and claim the 10–1000× spike matches ours.
  That would be a benchmark-tier claim requiring controlled
  training runs with full ablations.  v56 certifies the math of the
  channel; a separate `bench-v56-grokking` is the honest next step.
- We did **not** build a Core ML MLModel or dispatch through
  `_ANEClient`.  The ANE layout helper is a scaffold prerequisite;
  a future `native-m4` binding target can call into Core ML with
  the shapes we produce.
- We did **not** compose v56 with the v54 proconductor.  σ profiles
  per frontier model are an additional side-channel into `ipttt`;
  composing is a straightforward future wiring.
- We did **not** replace v34 epistemic / aleatoric σ decomposition.
  v56 is orthogonal — trajectory σ, not output-distribution σ.

## Why σ-Constitutional

Constitutions are rules every actor signs on to, including the
actor who enforces them.  v56's invariant — "any self-modification
must lower σ" — is signed on by the runtime itself: the
controller that allows the update is the same controller that
rolls it back when σ regresses.  No special-cased escape hatch.
No "let me just try anyway" path.  The transistor switches state
the same way whether the answer is easy or the update is
necessary: **1 = 1**.

## Reproducibility

```
git clone <repo> && cd creation-os-kernel
make check-v56
# ⇒ [v56 self-test] 56/56 PASS (sigma-Constitutional scaffold;
#    no network, no engine)
./creation_os_v56 --architecture
```

## References

- Beyond Outcome Verification: Verifiable Process Reward Models
  for Structured Reasoning.  arXiv:2601.17223 (2026).
- In-Place Test-Time Training.  arXiv:2604.06169 (April 2026).
- Test-Time Self-Reflection for Continual Reasoning Improvement
  (TTSR).  arXiv:2603.03297 (February 2026).
- Grokking as a Phase Transition between Competing Basins:
  a Singular Learning Theory Approach.  arXiv:2603.01192
  (March 2026).
- Why Grokking Takes So Long: A First-Principles Theory of
  Representational Phase Transitions.  arXiv:2603.13331
  (March 2026).
- Guided Verifier: Collaborative Multimodal Reasoning via Dynamic
  Process Supervision.  arXiv:2602.04290 (2026).
- Apple Neural Engine reverse-engineering — `_ANEClient`,
  `_ANECompiler`, `_ANEInMemoryModelDescriptor` private APIs;
  `m0at/ANEtransformers`, `jmanhype/ane-lora-training`
  open-source projects (2026).
- SME2 – AI Acceleration with Armv9 CPUs.  Arm Ltd. (2026).
