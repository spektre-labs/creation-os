# v190 σ-Latent-Reason — reasoning without token leakage

## Problem

v111.2 `reason` emits **visible** chain-of-thought tokens.
That leaks the internal policy (and burns context budget).

## σ-innovation

v190 moves reasoning into a **recurrent block in the hidden
space**:

```
h_0          = encoder(x)
h_{n+1}      = ρ · (h_n − h*) + h*         (contraction map)
σ_latent(n)  = ‖h_{n+1} − h_n‖ / ‖h_n‖
stop when σ_latent(n) < τ_converge  or  n = max_depth
y            = decoder(h_final)
```

Because ρ < 1, convergence is Banach-guaranteed and σ_latent
strictly decays — that decay **is** the halt signal. The per-
class contraction sets how much thinking each query gets:

| class | ρ | radius | typical n_iters |
|---|---|---|---|
| easy   | 0.05 | 0.3 | 2 |
| medium | 0.30 | 2.0 | 5 |
| hard   | 0.55 | 4.0 | 9 |

**Privacy:** no intermediate tokens are emitted.
`total_middle_tokens == 0` is checked by the merge-gate. Only
the final answer and σ are ever observable.

Integration with v189 TTC: v189 picks the compute budget;
v190 spends it in the latent space.

## Merge-gate

`make check-v190` runs
`benchmarks/v190/check_v190_latent_reason_convergence.sh`
and verifies:

- self-test PASSES
- σ_latent is strictly decreasing on every query
- ≥ 90 % of queries converge to σ_latent < τ_converge (= 0.01)
- mean_iters_hard / mean_iters_easy ≥ 3.0
- `total_middle_tokens == 0` (zero CoT leakage)
- byte-deterministic

## v190.0 (shipped) vs v190.1 (named)

| | v190.0 | v190.1 |
|-|-|-|
| Block | closed-form contraction | BitNet-2B layers 10–20 |
| Halt | ρ-driven σ_latent | learnt halt network over σ-traces |
| Decoder | identity | BitNet decoder on `h_final` |
| Streaming | batched | token-parallel, same guarantee |

## Honest claims

- **Is:** a deterministic latent-reason kernel whose σ_latent
  strictly decays, converges ≥ 90 % of the time below 0.01,
  hard queries use ≥ 3× more iterations than easy, and no
  intermediate tokens are ever emitted.
- **Is not:** a live wiring of BitNet-2B recurrent layers; the
  halt network and decoder path ship in v190.1.
