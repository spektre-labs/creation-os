# v270 — σ-TinyML (`docs/v270/`)

σ-gate on microcontrollers.  v137 AOT-compiled the
σ-gate to 0.6 ns on workstations; v270 carries the same
branchless, integer-friendly gate down to MCU class:
Cortex-M0+ / M4 / M7 and Xtensa ESP32.  Sensor fusion
turns three sensors into one σ, anomaly detection
compares σ to a baseline, and OTA calibration rewrites
τ without reflashing firmware.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### MCU footprint envelope (exactly 1 row)

| field                         | value | contract  |
|-------------------------------|------:|-----------|
| `sigma_measurement_bytes`     |   12  | == 12 (3 × f32) |
| `code_flash_bytes`            |  960  | ≤ 1024    |
| `ram_bytes_per_instance`      |   64  | ≤ 100     |
| `thumb2_instr_count`          |   20  | ≤ 24      |
| `branchless`                  |  true | == true   |

### MCU targets (exactly 4, canonical order)

| target              | cpu_mhz | supported |
|---------------------|--------:|:---------:|
| `cortex_m0_plus`    |  48     | ✅        |
| `cortex_m4`         | 168     | ✅        |
| `cortex_m7`         | 480     | ✅        |
| `xtensa_esp32`      | 240     | ✅        |

### Sensor fusion (3 sensors + 4 fixtures, τ_fusion = 0.30)

Rule: `σ_fusion = max(σ_temp, σ_humidity, σ_pressure)`
then `σ_fusion ≤ τ_fusion → TRANSMIT` else → `RETRY`.
Both branches fire.

| label    | σ_temp | σ_humidity | σ_pressure | σ_fusion | decision |
|----------|-------:|-----------:|-----------:|---------:|:--------:|
| `calm`   | 0.08   | 0.12       | 0.05       | 0.12     | TRANSMIT |
| `warm`   | 0.14   | 0.21       | 0.11       | 0.21     | TRANSMIT |
| `noisy`  | 0.28   | 0.47       | 0.18       | 0.47     | RETRY    |
| `storm`  | 0.62   | 0.44       | 0.51       | 0.62     | RETRY    |

### Anomaly detection (exactly 4 readings)

Rule: `anomaly == (σ_measured > σ_baseline + delta)`
with `σ_baseline = 0.15`, `delta = 0.20`.  Both branches
fire.

| tick | σ_measured | anomaly |
|-----:|-----------:|:-------:|
|  0   | 0.10       | —       |
|  1   | 0.18       | —       |
|  2   | 0.42       | ✅      |
|  3   | 0.68       | ✅      |

### OTA τ calibration (exactly 3 rounds)

Contract: every `applied == true`, every
`firmware_reflash == false` — calibration is a payload
update, not a flash rewrite.

| round | τ_new | applied | firmware_reflash |
|------:|------:|:-------:|:----------------:|
| 1     | 0.25  | ✅      | —                |
| 2     | 0.30  | ✅      | —                |
| 3     | 0.35  | ✅      | —                |

### σ_tinyml

```
σ_tinyml = 1 − (footprint_ok + targets_ok +
                sensors_ok + fusion_rows_ok +
                fusion_both_ok + anomaly_rows_ok +
                anomaly_both_ok + ota_rows_ok) /
               (1 + 4 + 3 + 4 + 1 + 4 + 1 + 3)
```

v0 requires `σ_tinyml == 0.0`.

## Merge-gate contract

`bash benchmarks/v270/check_v270_tinyml_mcu_sigma_gate.sh`

- self-test PASSES
- footprint envelope holds (12 B / ≤ 1024 B flash /
  ≤ 100 B RAM / ≤ 24 instr / branchless)
- 4 MCU targets canonical, all supported + cpu_mhz > 0
- 3 sensors canonical
- 4 fusion fixtures, both branches fire,
  `σ_fusion == max(...)`
- 4 anomaly rows, both branches fire, formula matches
- 3 OTA rounds, every `applied && !firmware_reflash`
- `σ_tinyml ∈ [0, 1]` AND `σ_tinyml == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed footprint / target /
  sensor / fusion / anomaly / OTA manifest with
  FNV-1a chain.
- **v270.1 (named, not implemented)** — physical MCU
  builds (Cortex-M0+ / ESP32 / STM32), measured
  flash + RAM + disassembly of `sigma_gate()`, live
  BLE / WiFi transmit gating, OTA payloads signed via
  v181 audit + v182 privacy.

## Honest claims

- **Is:** a typed, falsifiable MCU σ-gate envelope
  where footprint limits, sensor fusion, anomaly
  detection, and OTA calibration are merge-gate
  predicates.
- **Is not:** a live MCU build.  v270.1 is where the
  envelope meets silicon.

---

*Spektre Labs · Creation OS · 2026*
