# v165 — σ-Edge: IoT and edge-device support

## Problem

Creation OS assumes a 16 GB M3 MacBook.  Most of the world runs
on tighter hardware: a Raspberry Pi 5 has 8 GB, a phone has
3 GB of usable RAM, a Jetson Orin Nano has 8 GB shared across
CPU + GPU.  A single "it runs on my laptop" claim is not a
sovereignty claim.

## σ-innovation

- **`cos-lite` composition.**  Only three kernels ship on the
  edge: `v101` bridge + `v29` σ-stack + `v106` HTTP server.
  `v115` memory / `v150` swarm / `v127` voice are *opt-in* via
  `v162` σ-compose.

- **Five target profiles.**  `macbook_m3` (full reference),
  `rpi5`, `jetson_orin`, `android`, `ios` — each baked with
  arch, triple, available RAM, GPU/camera presence, default HTTP
  port, a Makefile target name, and a σ-scale.  `ios` is
  `supported_in_v0 = false` — roadmap honesty.

- **σ-adaptive τ.**  The smaller the device, the higher τ:
  `τ_edge = clamp(τ_default / max(available_ram / 8192, 0.125),
  0.15, 1.0)`.  A model running on a phone reaches for abstain
  faster than the same model on a server.  The device admits
  its limits.

- **Fit check.**  Given a 4-component RAM budget
  `(binary, weights, kvcache, sigma_overhead)`, the kernel
  emits a boot receipt declaring `fits`, `headroom_mb`, and a
  human-readable note.  `cos-lite` refuses to boot on a target
  it can't run.

- **Cross-compile matrix.**  Each profile exposes the Makefile
  target + toolchain triple + whether QEMU is needed in CI.

## Merge-gate

`benchmarks/v165/check_v165_edge_rpi5_smoke.sh` asserts:

1. self-test passes
2. all five profiles present with correct triples and make
   targets
3. `ios.supported_in_v0 == false`
4. τ_edge is monotone non-increasing in RAM (smaller device →
   larger τ)
5. `cos-lite` boots under the default budget on rpi5 /
   jetson_orin / android
6. `cos-lite` refuses on ios with a "reserved for v165.1" note
7. crosscompile flags rpi5 / jetson / android as
   `needs_qemu = true`
8. determinism: two listings byte-identical

## v165.0 vs v165.1

| Aspect | v165.0 (this release) | v165.1 (planned) |
|---|---|---|
| Profile table | Baked, 5 targets, JSON | Same, machine-detected from `/proc/cpuinfo` + `system_profiler` |
| Cross-compile | Recipe name declared | Real `cos-lite-rpi5` / `-jetson` / `-android` Makefile targets |
| CI | Native on macOS/Linux only | QEMU-ARM64 smoke in GitHub Actions |
| iOS | Profile reserved | Swift wrapper over v76 σ-Surface |
| Boot | Simulated receipt | Actual `cos-lite` boot on device |

## Honest claims

- **Lab demo (C)**: deterministic profile + fit + τ-adapter +
  cross-compile matrix.
- **Not yet**: real cross-compilation, real boot on an actual
  RPi5, QEMU CI, iOS wrapper.
- v165.0 *defines the shape* of the edge target space and the
  τ-adaptation rule; v165.1 *runs* it on silicon.
