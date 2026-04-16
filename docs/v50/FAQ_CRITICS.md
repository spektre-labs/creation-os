# FAQ for critics, professors, and skeptics (v50)

## “This is just a wrapper around bitnet.cpp”

Yes — where an external engine is used, that is deliberate. The value proposition in this repo is not “replace BitNet”; it is the **σ-measurement and gating layer** that sits on logits and can be integrated with multiple engines. BitNet can be the motor; σ is the instrument panel.

## “2B parameters can’t compete with 70B”

On raw coverage-style accuracy, that is usually true. The more relevant question for high-stakes settings is whether the system can **abstain** and whether **conditional accuracy** on answered items is high. Those metrics require a harness that logs abstention + correctness jointly — see `benchmarks/v50/FINAL_RESULTS.md` once `make v50-benchmark` is wired to real eval JSON.

## “σ is just entropy; that’s not new”

Entropy is one channel among several in the σ story across the lab stack (Dirichlet decomposition, multi-channel structure, FPGA-side variance proxy, etc.). What is uncommon is combining these with **explicit fail-closed gates** and an **assurance pack** (`make certify`) aimed at the σ-critical path.

## “Where are the benchmarks?”

`make v50-benchmark` writes:

- `benchmarks/v50/FINAL_RESULTS.md` (rollup)
- `benchmarks/v50/*.json` (explicit **STUB** placeholders until the engine harness exists)

Until weights + eval drivers are pinned, **do not** read STUB JSON as measured performance.

## “How is this different from conformal prediction / selective prediction?”

Conformal methods often rely on held-out calibration distributions; this repo’s lab direction emphasizes **per-token / per-step telemetry** and **engineering gates** that can be tested and (partially) verified independently of a particular sampling story. The exact mathematical relationship is documented in `docs/SIGMA_VS_FIELD.md` (I-tier positioning vs literature).

## “Can prompt injection bypass σ-gating?”

Prompt attacks primarily manipulate **inputs**. σ telemetry in the lab path is defined over **logits** (and related statistics), not over the raw prompt string. That does not make attacks impossible — it changes the attack surface. See `docs/v48/RED_TEAM_REPORT.md`.

## “Who are you to claim certification-grade software?”

The repo does **not** claim FAA/EASA certification. It claims **artifacts and commands exist** (`make certify`) aligned to DO-178C-style engineering discipline. Judge the evidence, not the author.

## “This is one person with no institution”

The verification commands do not check credentials. They check what is in the tree.

## “Why AGPL-3.0?”

See `LICENSE`. If you need a different license for a product integration, that is a separate commercial/legal discussion.

## “What’s the roadmap?”

See `CHANGELOG.md` and `docs/SIGMA_FULL_STACK.md`. Tier tags live in `docs/WHAT_IS_REAL.md`.
