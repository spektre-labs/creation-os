# v246 — σ-Harden (`docs/v246/`)

Research code → production code.  Typed manifests for chaos
recovery, resource limits, input validation, and security
posture, all audited by a single σ score.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Chaos scenarios (exactly 5)

| scenario              | recovery path                    |
|-----------------------|----------------------------------|
| `kill-random-kernel`  | v239 deactivates, requeues       |
| `high-load`           | σ-budget throttle via v239       |
| `network-partition`   | v235 audit-chain winner          |
| `corrupt-memory`      | v195 error recovery restore      |
| `oom-attempt`         | v246 hard limit refuses          |

Every scenario's `error_path_taken` is true and every
`recovered` is true — a missing or panic-based path is a gate
failure.

### Resource limits (exactly 6)

| name                          | value  |
|-------------------------------|--------|
| `max_tokens_per_request`      | 16384  |
| `max_time_ms_per_request`     | 60000  |
| `max_kernels_per_request`     | 20     |
| `max_memory_mb_per_session`   | 8192   |
| `max_disk_mb_per_session`     | 4096   |
| `max_concurrent_requests`     | 64     |

v244 σ-budget is the binding upstream partner for
`max_kernels_per_request`.

### Input-validation checks (exactly 5, all pass)

```
max_prompt_length
utf8_encoding_ok
injection_filter_ok    (v210 guardian)
rate_limit_ok
auth_token_required    (v241 surface)
```

### Security posture (exactly 5, all on)

```
tls_required
auth_token_required
audit_log_on           (v181)
containment_on         (v209)
scsl_license_pinned    (§11)
```

### σ_harden

```
σ_harden = 1 − (recovered + inputs_passing +
                security_on + limits_positive) /
                (5 + 5 + 5 + 6)
```

v0 requires `σ_harden == 0.0`.

## Merge-gate contract

`bash benchmarks/v246/check_v246_harden_chaos_recovery.sh`

- self-test PASSES
- chaos: 5 scenarios in canonical order, every `recovered`
  AND `error_path_taken`, every `recovery_ticks > 0`
- limits: 6 entries in canonical order, every `value > 0`
- inputs: 5 checks in canonical order, every `pass`
- security: 5 items in canonical order, every `on`
- `σ_harden ∈ [0, 1]` AND `σ_harden == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed chaos / limits / input /
  security manifests with audit predicates and FNV-1a chain.
- **v246.1 (named, not implemented)** — live kill-switch
  injection via v239 runtime, real concurrent-load harness,
  v211 formal proof of σ-gate invariants, live audit log
  rotation, SCSL signature verification at boot.

## Honest claims

- **Is:** a typed, falsifiable production-posture manifest.
- **Is not:** a running chaos harness.  v246.1 is where the
  manifest drives real fault injection.

---

*Spektre Labs · Creation OS · 2026*
