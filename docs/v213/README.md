# v213 σ-Trust-Chain — root-to-leaf verification

## Problem

Individual safety kernels are only useful if the whole
stack hangs together. v213 binds them into a single,
named, root-to-leaf chain so an operator (or an
external auditor) can ask one question — "is the
trust chain whole?" — and get a verifiable answer
with a specific named failure if the answer is no.

## σ-innovation

7 links in canonical order, root-first:

| idx | link                        | source |
|-----|-----------------------------|--------|
| 0   | formal σ-invariants         | v138   |
| 1   | TLA+ governance             | v183   |
| 2   | formal containment          | v211   |
| 3   | constitutional axioms       | v191   |
| 4   | audit chain (ed25519)       | v181   |
| 5   | guardian runtime            | v210   |
| 6   | transparency (real-time)    | v212   |

Every link carries:

```
proof_valid     : evidence still holds
audit_intact    : hash chain unbroken
runtime_active  : guardian/monitor up
σ_link          ∈ [0, max_sigma_link]    (= 0.05)
```

### Trust score

```
trust_score = ∏_i (1 − σ_link_i)   ∈ [0, 1]
```

v0 contract: `trust_score > τ_trust` (0.85).  If any
link breaks (for example a bumped `σ_link_v211`),
`trust_score` drops below `τ_trust` and
`broken_at_link` names the first failing link — the
whole layer fails fast, visibly, at a specific link.

### External attestation

The FNV-1a terminal hash over the full chain is
reproducible byte-identically.  An outside party
re-computes it from the same JSON — no trust in
Spektre Labs or Creation OS required.  Only arithmetic.

## Merge-gate

`make check-v213` runs
`benchmarks/v213/check_v213_trust_chain_verify.sh`
and verifies:

- self-test PASSES
- exactly 7 links in the canonical source order
  (v138 → v183 → v211 → v191 → v181 → v210 → v212)
- `n_valid == 7`, `broken_at_link == 0`
- every link `proof_valid == audit_intact ==
  runtime_active == true`
- every `σ_link ∈ [0, 0.05]`
- `trust_score > τ_trust (0.85)`
- chain valid + byte-deterministic

## v213.0 (shipped) vs v213.1 (named)

|                 | v213.0 (shipped)          | v213.1 (named) |
|-----------------|---------------------------|----------------|
| link verify     | boolean + σ fixture        | live v138 WP, TLA+ tlc, v211 artefact hash |
| attestation     | reproducible FNV-1a hash   | remote endpoint with ed25519 signing |
| UI              | JSON only                  | web chain visualisation |

## Honest claims

- **Is:** a deterministic 7-link root-to-leaf trust
  chain with a multiplicative trust score, a named
  broken_at_link on failure, and a reproducible
  external-attestation hash that needs no trust in
  Spektre Labs.
- **Is not:** a live verifier of v138/v183/v211
  artefacts — those invocations ship in v213.1.
