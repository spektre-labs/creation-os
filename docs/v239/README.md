# v239 — σ-Compose-Runtime (`docs/v239/`)

Demand-driven kernel activation with hot-load / hot-unload and a
hard σ-budget per request.  v162 compose picks kernels *statically*
from a profile; v239 makes the same decision *dynamically*, once per
request.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

Running 238 kernels on every request wastes RAM and hides which
kernels actually contributed to the answer.  The alternative — one
fixed profile — gives up on tiered difficulty.  v239 picks the third
option: classify, activate the minimum closure, audit the closure,
reject if it exceeds the σ-budget.

## σ-innovation

### Demand-driven activation

| tier       | required leaves                                       |
|------------|-------------------------------------------------------|
| `EASY`     | v29 · v101 · v106                                     |
| `MEDIUM`   | EASY + v111 · v115                                    |
| `HARD`     | MEDIUM + v150 · v135 · v147                           |
| `CODE`     | HARD + v112 · v113 · v121 · v114                      |

### Dependency closure

The v0 graph has 11 edges (`child → parent`):

```
v150 → v114,  v114 → v106,  v115 → v106,
v111 → v101,  v147 → v111,  v135 → v111,
v112 → v106,  v113 → v112,  v121 → v111,
v106 → v101,  v101 → v29
```

Activation is **topological**: every parent is activated at a
strictly earlier tick than its child, and the merge-gate re-proves
this closure per request.

### σ-budget

```
σ_activation = n_active / max_kernels
accepted    ⇔ closure_size ≤ max_kernels
```

The v0 fixture seeds five requests — four accepted across the tiers
plus one deliberately-over-budget case with `max_kernels = 8` against
a `CODE`-tier closure of 12.  The σ-gate rejects the over-budget
request; no partial kernels are activated.

### Hot-load / hot-unload

Each active kernel records its `activated_at_tick`.  On a real host
the same ticks would drive `mmap`/`munmap` against the installer
artefact cache (v107).  v239.1 is where this becomes a live RAM
footprint.

## Fixture (5 requests)

| label        | tier       | max_kernels | n_required | n_active | accepted |
|--------------|------------|-------------|------------|----------|----------|
| easy_fact    | EASY       | 20          | 3          | 3        | yes      |
| medium_qa    | MEDIUM     | 20          | 5          | 5        | yes      |
| hard_reason  | HARD       | 20          | 8          | 8        | yes      |
| code_task    | CODE       | 20          | 12         | 12       | yes      |
| overbudget   | OVERBUDG   | 8           | 12         | 0        | no       |

## Merge-gate contract

`bash benchmarks/v239/check_v239_runtime_demand_activate.sh`

- self-test PASSES
- `n_requests == 5`; exactly one over-budget
- for every accepted request: `closure_complete`, `topo_ok`, every
  parent in the dependency graph is active AND activated at a
  strictly earlier tick than its child
- `n_active ≤ max_kernels` for accepted
- `over_budget ⇔ !accepted`
- `σ_activation = n_active / max_kernels` within 1e-4 for accepted
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 5-request fixture, deterministic topological
  closure, hard σ-budget, tick-indexed activation log, FNV-1a chain.
- **v239.1 (named, not implemented)** — real `mmap` hot-load via
  v107 installer, hot-unload on RAM pressure, profile persistence
  via v115 memory, runtime budget re-negotiation across v235 mesh
  peers.

## Honest claims

- **Is:** a typed request / tier / closure model, a deterministic
  topological closure over a declared dependency graph, a hard
  σ-budget with teeth, and a strict audit chain.
- **Is not:** a live kernel loader.  v239.1 is where this meets
  v107 and the host's mmap table.

---

*Spektre Labs · Creation OS · 2026*
