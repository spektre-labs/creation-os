# v271 — σ-Swarm-Edge (`docs/v271/`)

σ-orchestrated sensor swarms.  A single sensor is a
node; tens of sensors over BLE / LoRa / Zigbee form a
swarm.  v271 types the swarm: consensus rejects high-σ
sensors, spatial anomaly triggers on σ-gradients,
energy-aware σ throttles sample rate, and a Creation OS
gateway bridges to v262 hybrid engine.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Mesh consensus (exactly 6 sensors, τ_consensus = 0.50)

Rule: `included == (σ_local ≤ τ_consensus)`; σ_swarm =
mean(σ_local over included); σ_raw = mean(σ_local over
all).  Contract: ≥ 1 excluded AND ≥ 1 included AND
σ_swarm < σ_raw.

| id       | σ_local | included |
|----------|--------:|:--------:|
| `node-A` | 0.08    | ✅       |
| `node-B` | 0.17    | ✅       |
| `node-C` | 0.21    | ✅       |
| `node-D` | 0.11    | ✅       |
| `node-E` | 0.78    | —        |
| `node-F` | 0.14    | ✅       |

### Distributed anomaly (exactly 4 fixtures, threshold = 0.25)

Rule: `spatial_anomaly == ((σ_center − σ_neighborhood) > 0.25)`.
Both branches fire.

| center   | σ_center | σ_neighborhood | anomaly |
|----------|---------:|---------------:|:-------:|
| `node-A` | 0.10     | 0.12           | —       |
| `node-B` | 0.18     | 0.15           | —       |
| `node-C` | 0.62     | 0.14           | ✅      |
| `node-D` | 0.55     | 0.22           | ✅      |

### Energy-aware σ (exactly 3 tiers, canonical)

`σ_energy` strictly ascending AND `sample_rate_hz`
strictly descending — hotter battery → faster sampling.

| tier      | σ_energy | sample_rate_hz |
|-----------|---------:|---------------:|
| `charged` | 0.05     | 100            |
| `medium`  | 0.40     |  10            |
| `low`     | 0.82     |   1            |

### Gateway bridge (exactly 1 fixture)

| field                | value            |
|----------------------|------------------|
| `gateway_device`     | `raspberry_pi_5` |
| `bridged_to_engine`  | `engram-lookup`  |
| `σ_bridge`           | 0.12             |
| `swarm_size_nodes`   | 6 (matches mesh) |
| `ok`                 | ✅               |

`bridged_to_engine` must be a valid v262 engine
(`bitnet-3B-local / airllm-70B-local / engram-lookup /
api-claude / api-gpt`).

### σ_swarm_edge

```
σ_swarm_edge = 1 − (consensus_rows_ok +
                    consensus_improves_ok +
                    anomaly_rows_ok + anomaly_both_ok +
                    energy_rows_ok + energy_monotone_ok +
                    gateway_ok) /
                   (6 + 1 + 4 + 1 + 3 + 1 + 1)
```

v0 requires `σ_swarm_edge == 0.0`.

## Merge-gate contract

`bash benchmarks/v271/check_v271_swarm_edge_consensus.sh`

- self-test PASSES
- 6 sensors; `included` matches σ vs τ_consensus = 0.50
- σ_swarm < σ_raw; both included and excluded fire
- 4 anomaly rows; formula matches; both branches fire
- 3 energy tiers canonical with σ_energy ascending AND
  sample_rate_hz descending
- Gateway `bridged_to_engine` in v262 set; swarm_size
  matches
- `σ_swarm_edge ∈ [0, 1]` AND `σ_swarm_edge == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed sensor / anomaly / energy
  / gateway manifest with FNV-1a chain.
- **v271.1 (named, not implemented)** — live BLE /
  LoRa / Zigbee mesh with v128 transport, real swarm
  consensus + σ-driven eviction, live gateway bridging
  to v262 hybrid engine producing measured
  end-to-end latency.

## Honest claims

- **Is:** a typed, falsifiable swarm manifest where
  consensus gain over naive mean, spatial anomaly,
  energy-aware sample-rate monotonicity, and gateway
  validity are merge-gate predicates.
- **Is not:** a running mesh of BLE / LoRa nodes.
  v271.1 is where the manifest meets a radio.

---

*Spektre Labs · Creation OS · 2026*
