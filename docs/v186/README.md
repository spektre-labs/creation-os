# v186 σ-Continual-Architecture — architecture that grows and prunes itself

## Problem

v163 evolve-architecture optimises the parameters of
existing kernels. v177 compress deletes kernels. Neither
**creates** new ones. A σ-governed system whose capacity is
fixed eventually runs out of room as new domains are added —
σ_mean drifts up and stays up.

## σ-innovation

v186 closes the loop: v133 meta watches σ_mean per domain;
when it trends up, the architecture is judged under-capacity
and v146 genesis proposes a new kernel; v163 evolve accepts
or rejects on Δσ; v177 compress prunes a weak kernel in an
over-capacity domain; every change is logged to v181 audit
so the architecture history is rewindable.

```
σ_history[domain][epoch]  ─►  slope  >  τ_growth  ⇒  starved
starved domain            ─►  genesis proposes kernel
evolve(Δσ < 0)            ─►  accept
over-capacity + weak      ─►  prune
every change              ─►  hash-chained history entry
```

v186.0 ships a deterministic, weights-free fixture: 6 initial
kernels across 4 domains, 6 epochs. Two domains drift upward
(starved), one is flat (stable), one drifts downward
(over-capacity).

## Merge-gate

`make check-v186` runs
`benchmarks/v186/check_v186_architecture_growth_cycle.sh`
and verifies:

- self-test
- 6 initial kernels
- ≥ 1 starved domain
- ≥ 1 grown kernel, ≥ 1 accepted
- ≥ 1 pruned kernel
- final kernel count ≠ initial (net architectural change)
- hash chain replays with zero mismatches
- chain tip non-zero and equals last entry
- output byte-deterministic

## v186.0 (shipped) vs v186.1 (named)

| | v186.0 | v186.1 |
|-|-|-|
| Monitor | in-process slope detector | v133 meta HTTP feed |
| Genesis | synthetic kernel name | real v146 genesis wiring |
| Evolve | closed-form Δσ acceptance | live v163 evolve loop |
| Prune | utility threshold | real v177 compress plan |
| Audit | FNV-1a hash chain | real v181 audit-signed chain |
| UI | CLI JSON | Web-UI architecture-history timeline |

## Honest claims

- **Is:** a deterministic continual-architecture controller that
  detects starved domains, grows new kernels, prunes weak
  ones, and records every decision in a verified hash chain.
- **Is not:** live wiring into v146/v163/v177/v181 or a real
  architectural search. The integrated loop ships in v186.1.
