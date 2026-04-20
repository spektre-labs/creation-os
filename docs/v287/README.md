# v287 — σ-Granite

**Material hardness in code: zero dependencies, C99 forever, platform-agnostic core.**

The pyramids lasted 4500 years because of the granite.  Code lasts
decades because every external import is rusting steel in the wall.
v287 types the longevity contract as four v0 manifests: dependency
ALLOW/FORBID polarity, language-standard whitelist, platform
coverage, and vendoring policy — so a build cannot "accidentally"
link a package manager into a kernel translation unit.

## σ innovation (what σ adds here)

> **σ_granite is a surface-hygiene residual: every deviation from
> the canonical ALLOW/FORBID polarities is visible as σ > 0.**  v0
> requires `σ_granite == 0.0`, so "zero dependencies" is a gate-
> enforced invariant, not a README aspiration.

## v0 manifests

Enumerated in [`src/v287/granite.h`](../../src/v287/granite.h); pinned
by
[`benchmarks/v287/check_v287_granite_zero_deps.sh`](../../benchmarks/v287/check_v287_granite_zero_deps.sh).

### 1. Dependency policy (exactly 6 canonical rows)

| name       | verdict  | in_kernel |
|------------|----------|-----------|
| `libc`     | `ALLOW`  | `true`    |
| `posix`    | `ALLOW`  | `true`    |
| `pthreads` | `ALLOW`  | `true`    |
| `npm`      | `FORBID` | `false`   |
| `pip`      | `FORBID` | `false`   |
| `cargo`    | `FORBID` | `false`   |

Contract: canonical names in order; every ALLOW row has
`in_kernel = true`, every FORBID row has `in_kernel = false`.

### 2. Language standard (exactly 3 canonical rows)

`C89 (1989, allowed=true)`, `C99 (1999, allowed=true)`, `C++ (1998,
allowed=false)`.  Contract: 2 allowed AND 1 forbidden.

### 3. Platform coverage (exactly 5 canonical targets)

`linux · macos · bare_metal · rtos · cortex_m`, each
`kernel_works == true` AND `ifdef_only_at_edges == true` — the
kernel core assumes no OS, hardware, or endianness.

### 4. Vendoring policy (exactly 3 canonical rows)

`vendored_copy` allowed in kernel; `external_linkage` and
`supply_chain_trust` forbidden.

### σ_granite (surface hygiene)

```
σ_granite = 1 −
  (dep_rows_ok + dep_allow_polarity_ok +
   dep_forbid_polarity_ok + std_rows_ok + std_count_ok +
   platform_rows_ok + platform_works_ok + platform_ifdef_ok +
   vendor_rows_ok + vendor_polarity_ok) /
  (6 + 1 + 1 + 3 + 1 + 5 + 1 + 1 + 3 + 1)
```

v0 requires `σ_granite == 0.0`.

## Merge-gate contracts

- 6 dep rows canonical; 3 ALLOW, 3 FORBID; polarities match.
- 3 standard rows canonical; C89 and C99 allowed; C++ forbidden.
- 5 platform rows canonical; every row works AND confines `#ifdef`
  to edges.
- 3 vendoring rows canonical; vendored copies allowed; external
  linkage and supply-chain trust forbidden.
- `σ_granite ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** names the dependency / standard / platform /
  vendoring policies as predicates.  No live SBOM, no vendored-hash
  auditor.
- **v287.1 (named, not implemented)** is `cos audit --deps` actually
  parsing translation units, emitting a live SBOM pinned to the
  6-row ALLOW/FORBID contract, plus boot-time hash verification of
  every vendored copy against its upstream at the pinned version.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).
