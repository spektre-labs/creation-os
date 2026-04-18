# v145 — σ-Skill · Atomic Skill Library

**Kernel:** [`src/v145/skill.h`](../../src/v145/skill.h), [`src/v145/skill.c`](../../src/v145/skill.c) · **CLI:** [`src/v145/main.c`](../../src/v145/main.c) · **Gate:** [`benchmarks/v145/check_v145_skill_acquire_and_route.sh`](../../benchmarks/v145/check_v145_skill_acquire_and_route.sh) · **Make:** `check-v145`

## Problem

The ICLR 2026 "Atomic Skill Acquisition" track consensus:
self-improving agents should learn **small reusable skills**
(≈ LoRA + template + self-test), not whole-model updates. In
Creation OS a skill is a triple

```
{LoRA-adapter, prompt-template, self-test}
```

stored on disk as:

```
skills/<name>.skill/
  adapter.safetensors      # LoRA, rank-R, ~200 KB
  template.txt             # "Solve step by step…"
  test.jsonl               # ≥ 5 eval items
  meta.toml                # σ_avg, pass-rate, learned_at
```

## σ-innovation

v145 is the in-memory, deterministic kernel that manages this
library. Five contracts enforced by the merge-gate:

| # | Contract | Semantics |
|---|----------|-----------|
| **S1** | `acquire(σ_avg ≥ τ_install)` ⇒ rejected | The skill is counted in `n_rejected` but not installed — σ gates the library. |
| **S2** | `route(topic)` returns argmin σ_avg | Among installed skills with that target_topic; -1 on miss. |
| **S3** | `stack(indices)` σ = min(σ_i) / √n | LoRA-merge shrinkage on weakly-correlated answers. |
| **S4** | `share(τ)` marks `σ_avg < τ` as shareable | v128 mesh gossip contract — only shareable skills leave the node. |
| **S5** | `evolve(idx, seed)` is monotone | Variants are installed iff `σ_new < σ_old` and bump `generation`; non-monotone regressions are never written back. |

σ_avg per skill is produced deterministically from `(seed,
difficulty)` — a weights-free stand-in for the real LoRA fit.
On the default seed `0xC05`, difficulties `{0.15, 0.05, 0.40,
0.22, 0.10}` reliably install all five topics (math, code,
history, language, logic).

## Merge-gate

`make check-v145`:

1. Self-test covers S1..S5 end-to-end (acquire / σ-gate / route
   hit + miss / stack shrinkage / share mark / evolve monotone).
2. `--demo --seed 0xC05` seeds 5 skills, stacks math+code,
   shares at τ=0.35, and evolves the math skill at least once.
3. `--route math`, `--route code` both hit; `--route
   metaphysics` misses cleanly (`hit:false`).
4. Byte-identical JSON across two runs at the same seed.

## v145.0 vs v145.1

* **v145.0** — in-memory, pure-C, stdlib-only, zero file I/O.
* **v145.1** — on-disk skills/\<name\>.skill/ layout; real LoRA
  adapter merge wired through the v124 trainer; v128 mesh gossip
  that replicates only shareable skills across nodes with σ-DP
  (v129) rate-limiting; CMA-ES (v136) driving the `evolve` step
  across `{rank, template-slot, threshold}`.

## Honest claims

* **σ_avg is synthetic in v145.0.** It is a deterministic
  function of `(seed, difficulty)` — not a measured evaluation.
  It exists so the library contract (install-gate, routing,
  shrinkage, share-gate, monotone evolve) can be proven on CI.
* **Stack shrinkage is an upper bound.** The `min(σ_i) / √n`
  formula is the conservative worst-case for weakly-correlated
  stacked answers; real LoRA-merge may or may not reach it. The
  merge-gate asserts the math, not the model.
* **Mesh sharing is symbolic.** `is_shareable` is a flag set on
  an in-memory array. The actual gossip path ships in v145.1.
