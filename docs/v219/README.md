# v219 — σ-Create (`docs/v219/`)

σ-governed creative generation: novelty, quality, surprise, and iterative
refinement, reported honestly.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

LLMs generate creative content *blindly*.  A single number (log-probability) is
often the only signal, and it measures **decoder confidence**, not
**creativity**.  High confidence can mean either "this is polished" or "this is
the safest possible completion".

v219 separates three orthogonal σ-measures per candidate — novelty, quality,
surprise — and lets the user tune the slider.

## σ-innovation

For every request we generate `N = 5` candidates across four modes:

- **TEXT**   — stories, poems, essays, dialogue
- **CODE**   — algorithms, architectures, patterns
- **DESIGN** — concepts, layouts, structures
- **MUSIC**  — melody sequences (MIDI / ABC notation)

Each candidate carries three σ-values:

- `σ_novelty_raw`  prior (heuristic or v125 history)
- `σ_quality_raw`  v150 debate prior
- `σ_surprise`     `clamp(1 − cos(in_embed, out_embed), 0, 1)` — a
  v126-embedding proxy, distinct from `σ_product`.

We then run iterative refinement:

- **debate** (v150) — 3 rounds of α=0.15 **clamped-positive** shrinkage of
  `σ_quality` toward the eligible-pool mean + 0.05.  "Clamped-positive" means
  debate can only *sharpen* quality, never dull it:
  `σ_quality_after ≥ σ_quality_before` by construction.
- **reflect** (v147) — 1 pass of α=0.10 shrinkage of `σ_novelty` toward the
  reflect mean.

The winner is `argmax(σ_novelty × σ_quality)` after refinement, subject to the
**novelty-vs-quality slider**:

- **SAFE**     — `σ_novelty ≤ τ_novelty_safe = 0.25` (polish, no risk)
- **CREATIVE** — `σ_novelty ≥ min_novelty_creative = 0.40` and
                `σ_novelty ≤ τ_novelty_creative = 0.85` (user signed up for risk)

## Merge-gate contract

`bash benchmarks/v219/check_v219_creative_novelty_quality.sh`

- self-test PASSES
- 8 requests × 5 candidates, 4 modes × 2 levels
- every SAFE winner `σ_novelty ≤ τ_novelty_safe`
- every CREATIVE winner `σ_novelty ∈ [min_novelty_creative, τ_novelty_creative]`
- refinement monotone per winner (`n_refine_monotone == 8`)
- `σ_surprise` cross-check passes on all 8 requests (matches `1 − cos(in, out)`)
- `impact == σ_novelty · σ_quality`
- chain valid + byte-deterministic across repeated runs

## v0 vs v1 split

- **v0 (this tree)** — deterministic fixture, closed-form σ updates,
  `clamped-positive` debate, no live embedding, no live debate agents.
- **v219.1 (named, not implemented)** — live v126 embedding for
  `σ_surprise`, live v150 debate agents, live v147 reflect, real mode-specific
  generators.

## Honest claims

- **Is:** a measurable separation of novelty, quality, and surprise with a
  user-controllable risk slider and auditable refinement.
- **Is not:** a proof that the winning candidate is *good*.  Taste sits
  outside σ.  v219 tells you how a candidate was scored; it does not tell
  you whether you'll like it.

---

*Spektre Labs · Creation OS · 2026*
