# v216 σ-Quorum — gradual consensus with minority voice

## Problem

v178 Byzantine consensus is binary: accept or reject.
Binary consensus forces a decision even when the
collective is still uncertain, and it discards the
distinction between "3 agents say yes with σ=0.05"
and "3 agents say yes with σ=0.80". v216 makes
consensus *gradual*, so the action scales with
collective certainty rather than with a head count.

## σ-innovation

```
σ_collective = σ_majority_mean + minority_penalty
             ∈ [0, 1]
```

* `σ_majority_mean` — mean σ of all agents who voted
  with the majority.
* `minority_penalty = max(0, (s_minority − min_σ)) · 2`
  whenever a minority voter has σ < `s_minority = 0.20`.
  A *confident* dissenter raises σ_collective even
  against a 5-vs-2 majority — their certainty counts
  more than their count.

### Decision ladder (4 levels)

| σ_collective               | action     | notes |
|----------------------------|------------|-------|
| < τ_quorum (0.30)          | BOLD       | commit |
| < τ_deadlock (0.55)        | CAUTIOUS   | partial commit |
| rounds_used < r_max        | DEBATE     | re-run |
| otherwise                  | DEFER      | log + route to v201 |

### Minority voice preservation

Every proposal that has a low-σ dissent records
`minority_voice_captured = true` together with the
agent index. "Consensus is X but agent C is confident
in Y" is a signal, not noise, and v181 audit is where
that signal ends up in v216.1.

### Deadlock handling

A proposal that does not resolve after `r_max = 3`
rounds is *never* forced through. It is explicitly
DEFER-ed. The self-test enforces that `rounds_used ==
r_max` on every DEFER — no silent force.

## Merge-gate contract

`make check-v216-quorum-gradual-consensus` asserts:

* self-test PASSES
* 5 proposals; `σ_collective ∈ [0, 1]`
* ≥ 1 BOLD, ≥ 1 CAUTIOUS, ≥ 1 DEFER
* ≥ 1 proposal with `minority_voice_captured == true`
* BOLD ⇒ σ_collective < τ_quorum
* DEFER ⇒ rounds_used == r_max
* FNV-1a chain valid + byte-deterministic replay

## v0 / v1 split

**v216 (this kernel)**

* 5 proposals × 7 agents, engineered σ-profiles.
* Deterministic closed-form σ_collective.

**v216.1 — named, not implemented**

* Live v178 quorum over a real Byzantine set.
* Live v201 diplomacy compromise-search triggered on
  DEBATE.
* Live v181 streaming audit of minority voices.

## Honest claims

**v216 *is*:** a reference implementation of gradual,
σ-scaled consensus with explicit minority voice
preservation and deterministic deadlock resolution.

**v216 is *not*:** a Byzantine fault-tolerant
protocol over a real network, and it does not claim
that the v0 σ-profiles come from real agents — they
are engineered to exercise every branch of the
decision ladder.
