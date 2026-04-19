# v251 — σ-Marketplace (`docs/v251/`)

Kernel registry.  Creation OS kernels are modular; v251 turns
them into a typed, σ-audited, shareable marketplace with a
hard publish contract.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Registry

```
registry.creation-os.dev
```

### Published kernels (v0 fixture, 5 entries)

Each kernel carries four quality axes in `[0, 1]` (higher =
better), a derived `σ_quality` = mean of the four axes, a
semver version, and explicit kernel-id dependencies:

| kernel         | deps (kernel ids)     | σ_quality |
|----------------|-----------------------|----------:|
| `medical-v1`   | `v115, v111, v150`    | 0.9025    |
| `legal-v1`     | `v135, v111, v181`    | 0.8875    |
| `finance-v1`   | `v111, v121, v140`    | 0.8600    |
| `science-v1`   | `v204, v206, v220`    | 0.9225    |
| `teach-pro-v1` | `v252, v197, v222`    | 0.8500    |

### Install (exactly 1 action)

```
cos install medical-v1
  → auto-resolve deps (v115, v111, v150)
  → all deps installed = true
  → medical-v1 installed = true
```

### Composition (exactly 1 pair)

```
compose medical-v1 + legal-v1  → "medical-legal"
σ_compatibility = 0.18
τ_compat        = 0.50
compose_ok      = σ_compatibility < τ_compat  → true
```

### Publish contract (exactly 4 items, all required AND met)

```
merge_gate_green
sigma_profile_attached
docs_attached
scsl_license_attached
```

No kernel publishes without all four.  SCSL stays pinned.

### σ_marketplace

```
σ_marketplace = 1 − (kernels_ok + install_ok + compose_ok +
                     publish_contract_ok) / (5 + 1 + 1 + 4)
```

v0 requires `σ_marketplace == 0.0`.

## Merge-gate contract

`bash benchmarks/v251/check_v251_marketplace_kernel_install.sh`

- self-test PASSES
- 5 canonical kernels, each with `σ_quality = mean(axes) ± 1e-4`
  and `n_deps > 0`
- install `medical-v1`: `deps_resolved == n_deps`, `installed`
- compose `medical-v1 + legal-v1`: `σ_compatibility < τ_compat`,
  `compose_ok`
- publish contract: all 4 items `required` AND `met`
- `σ_marketplace ∈ [0, 1]` AND `σ_marketplace == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed registry manifest with 5
  fixture kernels, one install, one composition, one
  publish contract, FNV-1a chain.
- **v251.1 (named, not implemented)** — live
  `registry.creation-os.dev`, signed package manifests,
  `cos search` / `cos install` / `cos publish` CLI, live
  quality-score ingestion from v247 harness runs.

## Honest claims

- **Is:** a typed, falsifiable marketplace manifest with a
  hard publish contract.
- **Is not:** a running registry.  v251.1 is where the
  manifest drives real `cos publish` traffic.

---

*Spektre Labs · Creation OS · 2026*
