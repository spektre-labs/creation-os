# v244 — σ-Package (`docs/v244/`)

243 kernels in source.  v244 turns them into a one-command install
on every major desktop / server platform, with a typed
`MINIMAL` / `FULL` profile split, a four-state first-run
sequence, and a σ-audited update / rollback loop.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Platforms (v0 manifest, four)

| platform | install command                      |
|----------|--------------------------------------|
| macOS    | `brew install creation-os`           |
| linux    | `apt install creation-os`            |
| docker   | `docker run spektre/creation-os`     |
| windows  | `winget install creation-os`         |

### Install profiles

| profile  | kernels                                    | RAM    | disk   |
|----------|--------------------------------------------|--------|--------|
| MINIMAL  | seed quintet `{v29, v101, v106, v124, v148}` | ~4 GB  | ~2 GB  |
| FULL     | every kernel in the stack (≥ 243)          | ~16 GB | ~15 GB |

Minimal runs on M1 Air, Raspberry Pi 5, older laptops.  Full gets
the ANE + AMX dispatch hints on M3 / M4.

### First-run sequence

```
cos start →
    SEED      ("Presence: SEED. Growing...")
    GROWING   (v229 seed grows the kernels)
    CHECKING  (v243 completeness check)
    READY     ("Ready. K_eff: 0.74. Kernels: N active.")
```

Strictly four states, strictly in that order; the merge-gate
rejects any deviation.

### Update / rollback

A v0 fixture runs four updates with `τ_update = 0.50`.  Any update
whose `σ_update` exceeds `τ_update` is rejected and the installed
version is left untouched.  The final step is a rollback drill that
restores the prior version byte-for-byte.

### σ_package

```
σ_package = 1 − (platforms_healthy / n_platforms)
```

v0 requires `σ_package == 0.0`: every platform carries a non-empty
install command and a well-formed manifest row.

## Merge-gate contract

`bash benchmarks/v244/check_v244_package_install_boot.sh`

- self-test PASSES
- exactly 4 platforms in canonical order, every `manifest_ok`
- minimal profile == `[29, 101, 106, 124, 148]`, `n_kernels == 5`
- full profile `n_kernels ≥ 243`
- first-run states `SEED → GROWING → CHECKING → READY` with
  strictly-ascending ticks
- updates: ≥ 3 accepted AND ≥ 1 rejected; rejects carry
  `σ_update > τ_update`
- `rollback_ok == true` and `installed_version == last.to`
- `σ_package ∈ [0, 1]` AND `σ_package == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed platform manifest, minimal / full
  profile split, four-state first-run, σ-audited update /
  rollback, FNV-1a chain.
- **v244.1 (named, not implemented)** — live Homebrew tap,
  Debian / RPM / Nix packaging, signed Docker manifests,
  Windows Store metadata, real `mmap`-based incremental update
  via v107 installer.

## Honest claims

- **Is:** a deterministic, typed, byte-replayable manifest of
  the install story — platforms, profiles, first-run sequence,
  update / rollback audit.
- **Is not:** a working `brew install`.  v244.1 is where the
  manifest drives a real package pipeline.

---

*Spektre Labs · Creation OS · 2026*
