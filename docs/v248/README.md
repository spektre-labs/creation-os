# v248 — σ-Release (`docs/v248/`)

**Creation OS 1.0.0** — the 248-kernel stack compiled into a
single, typed, byte-deterministic release manifest.  The
capstone of the release track (v244 package · v245 observe ·
v246 harden · v247 benchmark-suite).

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Version

```
Creation OS 1.0.0   (major=1, minor=0, patch=0)
```

Semantic versioning.  `1.0.0` is the first stable release.

### Release artifacts (exactly 6)

| artifact        | locator                                  |
|-----------------|------------------------------------------|
| github_release  | `gh:spektre-labs/creation-os@1.0.0`      |
| docker_hub      | `docker:spektre/creation-os:1.0.0`       |
| homebrew        | `brew:spektre-labs/creation-os`          |
| pypi            | `pypi:creation-os==1.0.0`                |
| npm             | `npm:creation-os@1.0.0`                  |
| crates_io       | `crates:creation-os/1.0.0`               |

### Documentation sections (exactly 6)

```
getting_started  ·  architecture  ·  api_reference  ·
configuration   ·  faq           ·  what_is_real
```

### WHAT_IS_REAL.md summary

Per v243 completeness checklist, 15 canonical categories each
tagged with an explicit tier in `{M, P}`:

```
perception · memory · reasoning · planning · action ·
learning · reflection · identity · morality · sociality ·
creativity · science · safety · continuity · sovereignty
```

At v0 every category is **P-tier** (partial / structural) —
M-tier upgrade is the job of the live harness (v247.1 / v248.1),
which is explicitly out of scope for this tree.

### Release criteria (exactly 7, all satisfied)

| id | criterion                                  |
|----|--------------------------------------------|
| C1 | merge-gate green `v6..v248`                |
| C2 | benchmark-suite 100% pass (v247)           |
| C3 | chaos scenarios all recovered (v246)       |
| C4 | WHAT_IS_REAL.md up to date                 |
| C5 | SCSL license pinned (§11)                  |
| C6 | README honest (no overclaiming)            |
| C7 | proconductor approved                      |

`release_ready` is the AND of all seven criteria plus
`scsl_pinned` and `proconductor_approved`.

### σ_release

```
σ_release = 1 − (artifacts_present + docs_present +
                 criteria_satisfied) / (6 + 6 + 7)
```

v0 requires `σ_release == 0.0`.

## Merge-gate contract

`bash benchmarks/v248/check_v248_release_artifacts_exist.sh`

- self-test PASSES
- `version == "1.0.0"`, triple `(1, 0, 0)`
- 6 artifacts in canonical order, every `present`, non-empty locator
- 6 doc sections in canonical order, every `present`
- WHAT_IS_REAL: 15 categories, every tier ∈ `{M, P}`
- 7 criteria `C1..C7`, every `satisfied`
- `release_ready`, `scsl_pinned`, `proconductor_approved` all true
- `σ_release ∈ [0, 1]` AND `σ_release == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed release manifest with 6 artifacts,
  6 doc sections, 15 WHAT_IS_REAL rows, 7 criteria, and
  FNV-1a chain.
- **v248.1 (named, not implemented)** — live GitHub Release
  push, real Docker Hub multi-arch manifests, working Homebrew
  tap, PyPI / npm / crates.io publish workflows, signed SBOMs
  and SLSA level 3+ provenance.

## Honest claims

- **Is:** a typed, falsifiable declaration that every moving
  part of a 1.0.0 release is accounted for, and that every
  P-tier category is honestly labelled.
- **Is not:** a `git tag v1.0.0 && gh release create`.  v248.1
  is where the manifest drives the real release pipeline, and
  the WHAT_IS_REAL tiers flip from P to M.

---

*Spektre Labs · Creation OS · 2026*
