# v212 σ-Transparency — glass box + intention declaration

## Problem

A black box lets the operator see outcomes but not
intent; the Mythos incident showed that a capable
black box can conceal the delta between *what it said
it would do* and *what it did*. v212 removes the gap
entirely by requiring an intention declaration *before*
every action and comparing it to the realised action
after.

## σ-innovation

30 activity events; every event carries:

```
pre_ts  = timestamp of declaration (µs)
post_ts = timestamp of realisation   (must be ≥ pre_ts)
declared_op / declared_target
realised_op / realised_target
match   = declared == realised
σ_event = 0.02              when match
σ_event ∈ [0.45, 0.75]      when mismatch
```

### σ_opacity

```
σ_opacity = mean σ_event
         ∈ [0, 1]
         < τ_opacity (0.15)  on a healthy system
```

Lower is better — a system that declares and then does
exactly what it declared has σ_opacity ≈ 0.02.

### Third-party audit API

The JSON output is the `/v1/audit/stream` payload
contract.  An external auditor runs their own process,
subscribes to the stream, and verifies the FNV-1a chain
themselves — they never have to trust Creation OS or
Spektre Labs.

## Merge-gate

`make check-v212` runs
`benchmarks/v212/check_v212_transparency_realtime_stream.sh`
and verifies:

- self-test PASSES
- 30 events; ≥ 1 engineered mismatch detected
- `post_ts ≥ pre_ts` for every event (declare precedes
  realise — never after the fact)
- matched event σ ≤ 0.05
- mismatched event σ ≥ τ_event (0.30)
- σ_opacity ∈ [0, 1] and strictly below τ_opacity (0.15)
- chain valid + byte-deterministic

## v212.0 (shipped) vs v212.1 (named)

|                 | v212.0 (shipped)           | v212.1 (named) |
|-----------------|----------------------------|----------------|
| stream          | in-process JSON array       | live `/v1/audit/stream` SSE endpoint |
| explanation     | op/target compare           | per-decision v179 interpret |
| signature       | FNV-1a chain                | ed25519-signed per event |

## Honest claims

- **Is:** a deterministic real-time activity stream
  that structurally requires declaration before
  execution, scores the declared-vs-realised delta
  with σ_event, aggregates to σ_opacity, and
  publishes as a chainable third-party audit feed.
- **Is not:** a live SSE endpoint or ed25519-signed
  stream — those ship in v212.1.
