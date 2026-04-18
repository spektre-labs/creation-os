# v175 σ-Debate-Train — debates as a training signal

## Problem

v150 lets N specialists argue.  v175 notices that the argument
is *structured data* — adversarial verification with a winner
and a loser — and turns it into DPO training data automatically.

## σ-innovation

Three mechanics, each σ-gated:

1. **Debate harvest.** 12 debates over 4 prompts × 3
   specialists.  Outcome per round:

   | rule                                    | class   |
   |-----------------------------------------|---------|
   | `σ_a − σ_b > 0.08`                      | **pair** — A rejected, B chosen |
   | otherwise, not consensus                | **chosen** — A survived critique |
   | `σ_a, σ_b < τ_consensus ∧ σ_Δ tiny`     | **skip** — uninformative |

2. **Self-play tournament.** 3-adapter round-robin, 6 matches
   (home + away).  Classic Elo:
   `expected = 1 / (1 + 10^((R_b − R_a) / 400))`,
   `new_R = R + K · (actual − expected)`, `K = 32`.
   The adapter with the lowest σ_base wins most matches and
   emerges champion.

3. **SPIN convergence.** Current-vs-previous generation with
   `σ_gen = 0.50 · 0.55^gen`.  The merge-gate asserts
   monotone non-increasing σ_current across generations; the
   loop stops when `σ_delta < 0.01` — the deterministic
   analogue of SPIN's "model stops learning from itself".

## Merge-gate

`make check-v175` runs
`benchmarks/v175/check_v175_debate_train_elo.sh` and verifies:

- self-test
- 12 debates with each of `{pair, chosen, skip}` ≥ 1x
- DPO NDJSON: no SKIPs leak; every PAIR has `σ_chosen < σ_rejected`
- tournament: 6 matches; champion = adapter 0 (lowest σ_base);
  sum of ratings stays within 50 of `3 · 1500`
- SPIN converges; `σ_current` is non-increasing
- JSON + DPO byte-identical across runs

## v175.0 (shipped) vs v175.1 (named)

| | v175.0 | v175.1 |
|-|-|-|
| Debates | hand-picked σ table | real v150 protocol over v114 swarm |
| Tournament | σ_base deterministic winner | real matches with LoRA adapter swap |
| SPIN | analytic σ-shrink | v124 hot-swap between generations |
| Trainer | no-op | v125 DPO |

## Honest claims

- **Is:** a deterministic kernel that demonstrates harvesting
  debates into DPO data, round-robin Elo updates and SPIN-
  style convergence — every invariant is offline-checkable.
- **Is not:** a trainer.  No weights move; the contract is
  the data shape, the Elo math and the convergence semantics.
