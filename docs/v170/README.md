# v170 σ-Transfer — cross-domain transfer with negative-transfer rollback

## Problem

v141 curriculum learns one subject at a time. Humans, by
contrast, reuse skill from a strong domain when they
encounter a weak neighbour (chemistry → biology). A naive
transfer can also make things **worse** (math → poetry), and
no v-kernel so far quantifies that risk.

## σ-innovation

v170 builds a **domain-embedding space** and a σ-gated
transfer protocol:

1. **8 baked domains** (`math`, `physics`, `chemistry`,
   `biology`, `code`, `logic`, `history`, `poetry`) sit at
   hand-picked coordinates in ℝ⁴ so near-and-far pairs are
   robustly ordered.
2. **Decision.** For a weak target, v170 picks the **closest**
   source with `σ_source ≤ τ_src` and `σ_source ≤ σ_target − δ`.
3. **Apply.** A closed-form outcome model moves σ by
   `Δ = −α·gap·exp(−d) + penalty(d > d_max)`. Improvement
   shrinks with embedding distance; crossing `d_max`
   **hurts** the target.
4. **Rollback (v124).** If `Δ > 0` (σ rose), the target’s σ is
   restored to the pre-transfer value and the attempt is
   recorded with a reason.
5. **Zero-shot.** An unseen target (`σ = 1.0`) pulls an
   ensemble σ from the k=2 nearest strong neighbours,
   weighted by `1/(d+ε)`.

Canonical cases from the fixture:

| Target    | Source chosen | σ_before → σ_after | Rollback |
|-----------|---------------|--------------------|----------|
| biology   | chemistry     | 0.710 → 0.471     | no       |
| history   | *(no source)* | no-op             | n/a      |
| physics†  | ensemble of math + code | σ_ensemble ≈ 0.28 | zero-shot |

† physics σ force-set to 1.0 to simulate an unseen domain.

## Merge-gate

`make check-v170` runs
`benchmarks/v170/check_v170_transfer_cross_domain.sh` and
verifies:

- 8 domains with 4-D embeddings and σ ∈ [0,1]
- `d(math,physics) < d(math,poetry)`
- biology transfer: source = chemistry, go = true,
  σ_after < σ_before, rollback = false
- history transfer never silently harms the target (either
  go = false with a reason, or go = true with
  σ_after ≤ σ_before)
- zero-shot on physics: k ≥ 1 and 0 < σ_ensemble < 1
- deterministic state JSON across runs

## v170.0 (shipped) vs v170.1 (named)

| | v170.0 | v170.1 |
|-|-|-|
| Embeddings | hand-picked 4-D | v126 σ-embed from real corpus |
| Transfer | closed-form σ-delta | real LoRA-adapter composition |
| Curriculum | standalone demo | v141 hooks σ-transfer on weakness detection |
| History | none | v115 memory of past attempts + lessons |

## Honest claims

- **Is:** a deterministic, offline-safe cross-domain
  transfer scheduler that demonstrates σ-gated source
  selection, distance-aware outcome prediction, rollback
  semantics and zero-shot ensembling.
- **Is not:** a fine-tuning pipeline. No weights move.
  v0 proves the *policy*; v170.1 wires the policy to
  LoRA adapters and v141 curriculum.
