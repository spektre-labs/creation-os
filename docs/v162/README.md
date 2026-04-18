# v162 — σ-Compose · Kernel Composition For The User

**Header:** [`src/v162/compose.h`](../../src/v162/compose.h) · **Source:** [`src/v162/compose.c`](../../src/v162/compose.c) · **CLI:** [`src/v162/main.c`](../../src/v162/main.c) · **Gate:** [`benchmarks/v162/check_v162_compose_profile_resolve.sh`](../../benchmarks/v162/check_v162_compose_profile_resolve.sh) · **Make:** `check-v162`

## Problem

Creation OS ships 163+ kernels. A user rarely needs all of them.
Someone running an on-device chat bot wants v101 + v106 + v111
and nothing else. A researcher wants memory + corpus + vision.
A sovereign deployment wants the whole stack. Without a first-
class composition layer, users have to read the kernel tree and
manually select their dependencies.

v162 makes the kernel set composable. Every kernel publishes a
manifest describing what it provides, what it requires, and its
σ-impact (added latency, added memory, σ-channels contributed).
Profiles declare roots; a resolver completes the dependency
closure. Hot-swap commands add or drop a kernel at runtime.

## σ-innovation

**σ-impact per kernel.** The manifest is the contract:

```
[provides]
swarm_debate = true
adversarial_verify = true

[requires]
v114_swarm = true
v106_server = true

[sigma_impact]
added_latency_ms = 50
added_memory_mb  = 60
sigma_channels   = ["sigma_collective", "sigma_consensus"]
```

The resolver computes total_latency_ms + total_memory_mb for the
enabled closure — so a user can see exactly what a profile costs
before enabling it.

## Profiles

| Profile | Roots | Typical closure size |
|---|---|---|
| `lean` | v101, v106, v111 | 3 kernels |
| `researcher` | + v115, v118, v119, v152 | ~7 kernels |
| `sovereign` | + v108, v117, v124, v128, v135, v140, v150, v153 | 15 kernels |
| `custom` | user-declared | user-declared + deps |

## Dependency rules

1. **Closure** — selecting `v150` pulls in `v128, v106, v111, v101`
   transitively.
2. **Acyclicity** — the composer runs a DFS cycle check on every
   call; cycles are reported as a hard error with the offending
   kernel name.
3. **Disable safety** — `cos disable v101` is rejected with
   `-2` if any currently-enabled kernel still requires `v101`.
   Leaves can always drop.

## Merge-gate

`make check-v162` asserts:

1. 7-contract self-test (C1..C7) passes.
2. `lean` profile resolves to ≤ 5 kernels and includes
   `{v101, v106, v111}`.
3. `sovereign` resolves to ≥ 12 kernels and includes
   `{v150, v124, v128, v152, v118, v140, v135}`.
4. `lean.n_enabled < sovereign.n_enabled` (different profiles
   produce different compositions — the user requirement).
5. `custom v150,v140` transitively pulls `v135`, `v128`, `v111`,
   `v106`, `v101`.
6. `cycle_detected == false` for every resolution.
7. `--enable v159` adds an enable event and toggles the flag.
8. `--disable v150` on sovereign drops v150 cleanly.
9. Byte-identical JSON for the same profile.

## v162.0 vs v162.1

* **v162.0** — in-process composer + event log; baked manifests
  for 17 representative kernels.
* **v162.1** — reads `kernels/vNN.manifest.toml` from disk for
  every kernel in the tree, drives real process-level enable/
  disable via a `POST /v1/compose` endpoint on v106, and
  streams events to v108 via WebSocket.

## Honest claims

* v162.0 does **not** start or stop real processes — it updates
  flags and event log. A "hot-swap" in v162.0 is an accounting
  change, not a systemd action.
* The baked manifest subset is a representative slice of the
  stack (one kernel per tier). v162.1 grows this to every
  shipped vNN.
* Dependency correctness is asserted by the gate for the baked
  subset; in v162.1 the TOML reader is the source of truth and
  it is asserted against the same closure invariants.
