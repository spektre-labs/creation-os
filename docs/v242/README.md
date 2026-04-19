# v242 — σ-Kernel-OS (`docs/v242/`)

The OS abstraction that turns the name "Creation OS" into the real
thing: typed processes, σ-priority scheduler, three IPC mechanisms,
a five-directory filesystem, a six-step boot sequence, and a
three-step graceful shutdown.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Process model

Every kernel is a typed process.  The scheduler orders the ready
queue by **σ ascending** (low σ = high priority): confident work
runs first, uncertain work yields.  The v0 fixture runs 12
processes covering the core reasoning, memory, and identity
kernels.

### IPC (three mechanisms)

```
MESSAGE_PASSING — typed envelopes between procs
SHARED_MEMORY   — v115 SQLite shared tape
SIGNALS         — v134 σ-spike signal
```

### Filesystem (five directories under `~/.creation-os/`)

```
models/   "programs"   weights, adapters
memory/   "data"       memories, ontology
config/                TOML configuration
audit/                 append-only audit chain
skills/   "apps"       installed skill packs
```

### Boot sequence (six ordered steps)

```
1. v29  σ-core
2. v101 bridge
3. v106 server
4. v234 presence check
5. v162 compose (profile load)
6. v239 runtime (activation)
```

After step 6 the OS is `READY` with ≥ 5 ready kernels including
the minimum set `{v29, v101, v106, v234, v162}`.  Further kernels
load on demand via v239.

### Shutdown sequence (three ordered steps)

```
1. v231 snapshot   persist state
2. v181 audit      log shutdown
3. v233 legacy     update legacy
```

### σ_os

```
σ_os = boot_step_fail_count / n_boot_steps
```

The v0 boot path requires `σ_os == 0.0` and final `state == STOPPED`
after a clean shutdown.

## Merge-gate contract

`bash benchmarks/v242/check_v242_kernel_os_boot_sequence.sh`

- self-test PASSES
- `n_processes == 12`; priorities form a dense permutation of
  `[0, 12)` and equal the argsort of σ ascending
- exactly 3 IPC mechanisms named `MESSAGE_PASSING`, `SHARED_MEMORY`,
  `SIGNALS` in that order
- exactly 5 FS dirs named `models`, `memory`, `config`, `audit`,
  `skills`, all prefixed `~/.creation-os/`
- boot sequence = 6 steps, kernel order `29 → 101 → 106 → 234 →
  162 → 239`, every step `ok`
- `n_ready_kernels ≥ 5` and includes `{29, 101, 106, 234, 162}`
- shutdown = 3 steps, kernel order `231 → 181 → 233`, every step `ok`
- `σ_os ∈ [0, 1]` AND `σ_os == 0.0`; `state == STOPPED`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed process / IPC / FS / boot / shutdown
  model with σ-scheduler, strict audit predicate, FNV-1a chain.
- **v242.1 (named, not implemented)** — real fork/exec into v107,
  POSIX signal bridge for v134, userspace filesystem mount, cgroup
  / sandbox integration with v113.

## Honest claims

- **Is:** a deterministic, typed model of an OS boundary over the
  Creation OS kernel stack — scheduler, IPC, FS, boot, shutdown.
- **Is not:** a kernel image.  v242.1 is where these typed steps
  become real OS calls.

---

*Spektre Labs · Creation OS · 2026*
