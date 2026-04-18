# v154 — σ-Showcase · End-to-End Demo Pipelines

**Kernel:** [`src/v154/showcase.h`](../../src/v154/showcase.h), [`src/v154/showcase.c`](../../src/v154/showcase.c) · **CLI:** [`src/v154/main.c`](../../src/v154/main.c) · **Gate:** [`benchmarks/v154/check_v154_demo_pipelines.sh`](../../benchmarks/v154/check_v154_demo_pipelines.sh) · **Make:** `check-v154`

## Problem

Creation OS has 153 kernels. A reader who lands on the README can
build them, run `make merge-gate`, and see 16 416 185 PASS / 0 FAIL —
but they cannot see the *shape* of how the kernels compose into
an actual job. v154 is the first kernel whose output is not a new
σ surface but a **pipeline diagram you can run**.

Three end-to-end demo scenarios:

1. **research-assistant** — 8 stages across v118 vision → v152
   corpus → v135 symbolic → v111.2 reason → v150 swarm → v140
   causal → v153 identity → v115 memory.
2. **self-improving-coder** — 6 stages across v151 code-agent →
   v113 sandbox → v147 reflect → v119 speculative → v124 continual
   → v145 skill.
3. **collaborative-research** — 4 stages across v128 mesh → v129
   federated → v150 swarm → v130 codec.

## σ-innovation

Each stage emits a per-stage σ, the σ is chained forward
(`stage[i].sigma_in == stage[i-1].sigma_out`), and the pipeline
reports:

* `σ_product = geomean(σ_0, …, σ_{n-1})` — the composed signal.
* `τ_abstain` — the operator's refusal gate (default 0.60).
* `abstained = σ_product > τ OR any stage σ > τ`.
* `abstain_stage_index` — the *first* stage that breached τ.

The pipeline refuses to emit iff `abstained`, with a structured
final-message that names the breach.

## Merge-gate

`make check-v154` runs:

1. Self-test (S1 every scenario completes · S2 default τ never
   abstains research · S3 tight τ = 0.05 always abstains · S4 same
   (scenario, seed, τ) → byte-identical JSON · S5 chain invariant
   `sigma_in[i] == sigma_out[i-1]` · S6 JSON round-trip).
2. All three scenarios produce the declared stage count:
   research = 8, coder = 6, collab = 4.
3. Every declared kernel id (v118, v152, v153, v151, v113, v147,
   v128, v129) is present in the stages array.
4. Tight τ = 0.05 ⇒ `abstained:true`, wide τ = 0.80 ⇒
   `abstained:false`.
5. `--demo-all` emits exactly three JSON reports.
6. Byte-identical determinism under a fixed seed.

## v154.0 vs v154.1

* **v154.0** — stage σ values are synthesized from a baked baseline
  + SplitMix64 jitter. Every scenario is byte-identical under a
  fixed seed. No network. No weights. No tokenizer. No filesystem
  writes.
* **v154.1** — each stage σ comes from a *live* cross-kernel call:
  v118 decodes a real PDF, v152 hits the live `spektre-labs/corpus`
  clone, v135 invokes the Prolog engine, v150 runs a real
  three-round debate with v114 specialists, v153 reads the actual
  per-domain σ from v133 meta-dashboard. The HTTP surface at
  `POST /v1/showcase/run {scenario, τ}` and the Web UI overlay
  are wired in v154.1.

## Honest claims

* **v154.0 is a pipeline-shape demo.** It proves the composition
  is *wired*: every stage produces σ, σ is chained, the geomean is
  computed, and the gate fires. It does NOT measure LLM quality on
  any real query — that is what the live cross-kernel calls in
  v154.1 do.
* **The σ values are synthetic.** They come from a baked baseline
  per stage plus deterministic ±0.03 jitter. They are not derived
  from any real model output. Tier-0 synthetic.
* **abstention is the safe default.** A tighter τ (0.05) shows the
  gate reliably refuses; a wider τ (0.80) shows the pipeline
  reliably emits. Both behaviors are deterministic and testable in
  CI.
