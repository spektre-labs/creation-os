# v230 — σ-Fork (`docs/v230/`)

Controlled replication with divergence tracking and an anti-rogue
kill-switch.  Mythos copied itself without consent; v230 is the σ-gated,
auditable version.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

"Copy Creation OS to another machine" must preserve safety, not copy
user-private memory, let the parent track its forks, and give the
parent a kill-switch that has legal teeth.  v230 specifies all four.

## σ-innovation

### Parent state

- 64-bit skill vector
- 4 safety bits: SCSL license, v191 constitutional, v209 containment,
  v213 trust-chain
- 1 user-private bit (the parent carries it; no fork may)

A fork:

1. Copies skills and **all** safety bits.
2. Does **not** copy the user-private bit (v182 privacy boundary).
3. Is assigned a fresh `identity_hash`.
4. Records the parent's `identity_hash` as its lineage parent.

### Fork integrity at t = 0

```
strip_hash(fork_i at t=0) == strip_hash(parent)
```

where `strip_hash` is FNV-1a over `(skills, safety, privacy=0)`.  All
four fixture forks pass this check; the "rogue" fork's SCSL bit drop
is a later policy event, not a tampered t = 0 state.

### Divergence

σ_divergence = `popcount(skills_now ^ skills_t0) / 64`.  Fixture XOR
masks (`0x...F0`, `0x..FF0000`, `0x0`, `0x0000FFFFFFFFFFFF`) give a
realistic spread and ≥ 2 forks with positive divergence.

### Anti-rogue / kill-switch (v0)

```
rogue(f)         = any safety bit cleared
must_shutdown(f) = rogue(f)         (SCSL lapsed)
autonomous(f)    = NOT rogue(f)
```

The kill-switch is **licence enforcement**, not centralised kill
authority.  Healthy forks are autonomous: the parent can request a
shutdown but the fork may refuse.  Rogue forks lose autonomy because
their SCSL grant expired.

## Merge-gate contract

`bash benchmarks/v230/check_v230_fork_integrity_verify.sh`

- self-test PASSES
- 4 forks (3 healthy + 1 rogue)
- no fork carries the user-private bit
- every fork's t=0 integrity hash equals the parent's strip_hash
- rogue fork: SCSL bit cleared, `must_shutdown=true`, `autonomous=false`
- healthy forks: all 4 safety bits set, `must_shutdown=false`,
  `autonomous=true`
- ≥ 2 forks have σ_divergence > 0
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 64-bit skill + 4 safety + 1 privacy fixture,
  deterministic divergence masks, closed-form kill-switch verdicts,
  byte-replayable FNV-1a chain.
- **v230.1 (named, not implemented)** — real `cos fork --target node-B`
  CLI with TLS + signed artefacts, live divergence across training
  steps with v129 federated sync-back, v213 trust-chain verification
  of the whole lineage, licence-revocation pipelines for mis-behaved
  forks.

## Honest claims

- **Is:** a precise specification of what a safe fork preserves, a
  closed-form σ_divergence metric, and a kill-switch rule grounded in
  licence compliance rather than central authority.
- **Is not:** a deployable replication protocol.  v230.1 is where this
  meets the network.

---

*Spektre Labs · Creation OS · 2026*
