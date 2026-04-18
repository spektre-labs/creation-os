# v164 — σ-Plugin: third-party extension host

## Problem

Creation OS is modular (v162 σ-compose).  But modularity only
helps when the module boundary is *open*: third parties must be
able to ship kernels, tools, and skills without forking the core.

A naive plugin system opens three holes at once:

1. **ABI drift** — every release breaks plugins.
2. **Weight exfiltration** — a plugin reads `model.safetensors`.
3. **Quality collapse** — a bad plugin silently drags σ down.

v164 closes all three in C.

## σ-innovation

- **Stable C ABI.**  Three entrypoints: `init` / `process` /
  `cleanup`.  The input is a `cos_v164_input_t` with the
  prompt + σ-profile; the output is a `cos_v164_output_t` with
  a response + a *plugin-reported* σ value.  A plugin can force
  an abstain by setting `sigma_override = true`.

- **Manifest-gated sandbox.**  Every plugin declares
  `required_caps` (bitmask: NETWORK / FILE_READ / FILE_WRITE /
  SUBPROCESS / MEMORY_READ).  Invocation grants a subset; a
  missing bit is refused with `σ_plugin = 1.0` (hard abstain).
  `MODEL_WEIGHTS` is a permanent deny — the host never grants it.
  `timeout_ms` is a hard kill.

- **σ_reputation as a ring buffer.**  Each plugin keeps the last
  16 σ_plugin observations; `σ_reputation = geomean(history)`.
  A plugin that abstains often (σ ≈ 1) climbs the reputation
  away from zero and eventually exits the "trusted" tier.

- **Four official plugins baked in** — `web-search`,
  `calculator`, `file-reader`, `git` — each with a deterministic
  simulated `process` that parameterizes latency / σ_plugin from
  a SplitMix64 stream keyed by `(seed, n_invocations)`.

## Merge-gate

`benchmarks/v164/check_v164_plugin_load_unload.sh` asserts:

1. `--self-test` returns 0
2. the registry lists four official plugins, each with a
   `sigma_impact` and `sigma_reputation`
3. sandbox refuses a plugin when a required cap isn't granted
   (`status == cap_refused`, `σ_plugin == 1.0`)
4. no plugin declares `COS_V164_CAP_MODEL_WEIGHTS`
5. `calculator "2+2"` → `"4"` deterministically
6. `--disable NAME` hot-swaps `enabled=false`
7. σ_reputation stays in `[0, 1]` and is reproducible across runs
8. two invocations produce byte-identical JSON

## v164.0 vs v164.1

| Aspect | v164.0 (this release) | v164.1 (planned) |
|---|---|---|
| Loader | Baked dispatcher, 4 plugins compiled in | `dlopen("lib*.dylib")` + `dlsym` |
| Sandbox | In-process timeout + cap mask | `fork()` + `seccomp-bpf` / `sandbox_init` |
| Install | `registry_install(manifest)` in-memory | `cos plugin install github.com/...` |
| Hot-swap | `set_enabled(name, bool)` | Live reload of a replacement `.dylib` |
| Third-party | Synthetic `run_unknown_default` | Actual ABI call into the loaded `.dylib` |

## Honest claims

- **Lab demo (C)**: deterministic ABI exerciser, sandbox-policy
  simulator, σ_reputation accountant.
- **Not yet**: real `dlopen`, real OS-level sandboxing, real
  third-party plugins shipped from a registry.
- v164.0 *defines the contract* and proves it deterministically.
  v164.1 *executes* the contract in production.
