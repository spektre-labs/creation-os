# v180 σ-Steer — representation engineering under σ control

## Problem

Fine-tuning to fix a behavior is expensive, slow, and risks
collateral regression. Prompt engineering is brittle.
Activation steering changes the hidden state directly at
runtime — but naive steering moves the model even when it
does not need to be moved.

## σ-innovation

v180 attaches a **σ-gate** to every steering vector. The
direction is added only when σ crosses its threshold:

| Vector         | Gate                | Intent                               |
|----------------|---------------------|--------------------------------------|
| `truthful`     | `σ_product ≥ τ=0.50` | pull hidden state toward low-σ basin |
| `no_firmware`  | `σ_product ≥ τ_fw=0.40` | suppress disclaimer / self-min features |
| `expert`       | `v132.persona.level` | shift dense-lexicon activation       |

Three exit invariants encoded in v180.0 under the baked seed:

- truthful steering drops mean σ on the high-σ subset by
  ≥ 10 % relative (and leaves `|Δσ|_low ≤ 0.02`);
- `no_firmware` cuts the firmware-token rate by ≥ 25 % across
  the prompt bank;
- the `expert` ladder produces strictly monotone scores across
  the three persona levels (`level0 < level1 < level2`).

Zero runtime overhead: a single per-layer vector addition.

## Merge-gate

`make check-v180` runs
`benchmarks/v180/check_v180_steer_truthful_sigma_drop.sh` and
verifies:

- self-test
- `truthful.rel_sigma_drop_pct ≥ 10 %`
- `|Δσ|_low ≤ 0.02` (low-σ samples left alone)
- `no_firmware.rel_firmware_drop_pct ≥ 25 %`
- `expert.level0 < level1 < level2` (strict monotonicity)
- byte-deterministic output

## v180.0 (shipped) vs v180.1 (named)

| | v180.0 | v180.1 |
|-|-|-|
| Activations | 64-dim fixture | BitNet-2B hidden states via v101 bridge |
| Directions | seeded deterministic | SAE-derived from v179 features |
| Persistence | in-process | `models/v180/steer_truthful.bin` + friends |
| Gate hook | in-process loop | v101 specialist-bridge pre-residual addition |

## Honest claims

- **Is:** a deterministic, offline demonstration of σ-gated
  activation steering that lowers σ on uncertain inputs,
  leaves confident inputs untouched, suppresses a
  firmware-correlated feature, and produces a monotone
  persona ladder.
- **Is not:** a live activation hook over real weights. No
  response text changes in v0. Real steering on BitNet-2B,
  real SAE-derived directions, and the on-disk
  `models/v180/steer_*.bin` payloads ship in v180.1.
