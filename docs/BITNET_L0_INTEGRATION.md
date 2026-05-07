# BitNet / bitnet.cpp and Creation OS L0

**Location of upstream sources:** Git submodule at `third_party/bitnet` ([Microsoft BitNet](https://github.com/microsoft/BitNet)). Do **not** fork the upstream `README.md` inside that submodule for Creation OS prose — it tracks Microsoft’s release and would complicate updates.

This file is the **canonical English integrator note** for this repository.

## Scope and claim discipline

- **BitNet b1.58** (research / product line) and **bitnet.cpp** are **third-party**. Throughput, energy, and comparison claims in Microsoft’s README and papers are **upstream** — not Creation OS harness results.
- For evidence hygiene see [`CLAIM_DISCIPLINE.md`](CLAIM_DISCIPLINE.md), `make bench`, and [`REPRO_BUNDLE_TEMPLATE.md`](REPRO_BUNDLE_TEMPLATE.md). Do **not** merge corpus marketing lines with measured headlines.

## Conceptual L0 stack (this repo)

- **Quantized / ternary-oriented inference:** use **bitnet.cpp** inside `third_party/bitnet` per upstream build instructions.
- **σ-gate:** score **prompt + response** in Python (default L1 entropy core) or HTTP API when serving.
- **Tiny / MCU:** deterministic helpers include `hw/tinyml/sigma_gate_tiny.h` and the Arduino sketch `scripts/hardware/esp32_sigma.ino`. These do **not** modify [`python/cos/sigma_gate.h`](../python/cos/sigma_gate.h) (packaging hook).

## Suggested pipeline

1. Run BitNet / bitnet.cpp to produce a response.
2. Score with Creation OS, e.g. `cos gate --prompt "<prompt>" --response "<response>"` or `POST /v1/score` via `cos serve`.
3. Apply policy: high σ → RETHINK / ABSTAIN / escalate; low σ → ACCEPT only per your tier.

## Optional clone

```bash
git clone --recursive https://github.com/microsoft/BitNet.git third_party/bitnet
cd third_party/bitnet
# Continue with upstream conda/cmake/model steps (see their README.md).
```

## Illustrative targets (not benchmark receipts)

| Target | Stack | σ-gate |
|--------|--------|--------|
| ESP32-S3 | UART lab sketch | L1-style entropy (`scripts/hardware/esp32_sigma.ino`) |
| SBC | bitnet.cpp GGUF | Python L1 |
| Apple Silicon | bitnet.cpp | L1–L3 optional |
| NVIDIA | bitnet.cpp GPU path | L1–L5 with probes |

## RISC-V

Specification-only note: [`RISC_V_XSIGMA.md`](RISC_V_XSIGMA.md).

**NOT AGI ACHIEVED.**
