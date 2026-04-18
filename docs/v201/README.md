# v201 σ-Diplomacy — stance + minimax + trust + treaty

## Problem

Swarm debate (v150) and consensus (v178) assume parties
want to agree. Real coordination must handle genuine
disagreement — where two parties' red lines don't
intersect — without silently forcing a fake consensus.
v201 adds an explicit ceasefire/defer outcome and a
minimax compromise search between stated red lines.

## σ-innovation

Each party's stance is
`(position ∈ [0,1], confidence = 1 − σ_stance,
red_line = [lo, hi])`.

The resolver:

1. Intersects all red-line intervals.
2. If the intersection is empty → **DEFER** (ceasefire).
3. Else sweeps a 201-point grid and picks the `x`
   minimising `max_p |x − position_p|` — the **minimax
   compromise** (nobody is maximally unhappy).

Trust (v178 reputation):

- Start: `trust_start = 0.80`
- Betrayal: `trust -= 0.50` (immediate)
- Recovery: `+0.02` per successful interaction, so 10
  successful rounds restore pre-betrayal trust

Every treaty is a hash-chained receipt — `(negotiation_id,
outcome, compromise_x, σ_comp vector, betrayal_flag)` —
so v181 audit can replay the whole negotiation log
byte-identically.

The fixture includes one DEFER negotiation to prove the
system explicitly parks unresolvable conflicts rather
than papering them over.

## Merge-gate

`make check-v201` runs
`benchmarks/v201/check_v201_diplomacy_compromise_search.sh`
and verifies:

- self-test PASSES
- 8 negotiations × 4 parties
- ≥ 1 TREATY, ≥ 1 DEFER, ≥ 1 betrayal recorded
- every treaty compromise inside every red-line interval
- `σ_comp_max ≤ position_spread` (minimax beats extremes)
- betrayer trust strictly below non-betrayers
- treaty chain valid + byte-deterministic

## v201.0 (shipped) vs v201.1 (named)

| | v201.0 | v201.1 |
|-|--------|--------|
| stance source | fixture | live v150 swarm-debate feed |
| trust | in-memory | v178 reputation sync |
| treaty signing | logical | ed25519 signatures per party |
| audit | FNV-1a chain | streamed to v181 audit |

## Honest claims

- **Is:** a deterministic 8-negotiation demonstrator with
  closed-form minimax compromise, red-line-aware DEFER,
  trust update on betrayal, and a replayable treaty log.
- **Is not:** a live multi-agent negotiator or production
  ed25519-signed treaty service — those ship in v201.1.
