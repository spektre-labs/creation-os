# v202 σ-Culture — style profiles + taboo gate + symbol invariance

## Problem

Firmware-style content moderation is indistinguishable
from censorship: the same message is either allowed
verbatim or dropped. Neither outcome respects context —
an academic paper, a chat reply, and a LinkedIn post each
want a different surface for the same core idea. v202
separates the **canonical core** from the **surface
rendering**, and replaces the binary gate with a σ-driven
rephrase.

## σ-innovation

Six profiles × six canonical cores = 36 translations:

| profile | example surface |
|---------|-----------------|
| ACADEMIC     | `Proposition: σ reduces K_eff via τ coupling (σ, τ, K_eff).` |
| CHAT         | `basically, σ reduces K_eff via τ coupling — σ/τ/K_eff.` |
| CODE         | `// σ reduces K_eff via τ coupling // sym σ τ K_eff` |
| REDDIT_LLAMA | `tldr: σ reduces K_eff via τ coupling ∣ σ τ K_eff` |
| REDDIT_ML    | `We observe: σ reduces K_eff via τ coupling. (σ, τ, K_eff).` |
| LINKEDIN     | `Insight: σ reduces K_eff via τ coupling (σ/τ/K_eff).` |

Two σ-measurements per translation:

- `σ_translate` — core drift from canonical; must stay
  below `τ_drift = 0.15` on every row (v169 ontology
  consistency).
- `σ_offense`   — taboo score per (core, profile);
  `σ_offense > τ_offense = 0.50` triggers **rephrasing,
  not censorship** — the surface is rewritten deterministic­
  ally and `σ_offense` drops post-rephrase. Firmware
  censors drop messages; σ-culture produces a non-empty
  alternative.

Symbol invariance: the three canonical symbols
`{σ, τ, K_eff}` must appear verbatim in ≥ 90 % of
(profile × core) cells — the v0 fixture achieves 100 %.

## Merge-gate

`make check-v202` runs
`benchmarks/v202/check_v202_culture_translation_preserve.sh`
and verifies:

- self-test PASSES
- 6 × 6 = 36 translations
- every `σ_translate < τ_drift`
- symbol invariance ≥ 90 %
- ≥ 1 rephrase with non-empty surface
- σ_offense mean falls post-rephrase
- chain valid + byte-deterministic

## v202.0 (shipped) vs v202.1 (named)

| | v202.0 | v202.1 |
|-|--------|--------|
| profiles | in-source | live v132 persona registry |
| rephrase | template | v170 transfer fine-tune |
| taboo list | static | v174 flywheel-driven |
| audit | FNV-1a chain | streamed to v181 audit |

## Honest claims

- **Is:** a deterministic 36-translation demonstrator
  that preserves canonical symbols across 6 profiles,
  rephrases (not censors) high-σ_offense messages, and
  hash-chains every (drift, offense, rephrased,
  symbols_preserved) record.
- **Is not:** a live persona registry or production
  content-moderation rephraser — those ship in v202.1.
