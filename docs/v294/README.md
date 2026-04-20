# v294 — σ-Federated

**Federated learning, σ-gated.**

Devices train locally, only updates travel. σ makes those updates
honest: low-σ updates pull more of the aggregate, high-σ updates
are quarantined, DP noise is named as a regime instead of a knob,
non-IID is routed per device, and the sovereign mesh rejects
untrusted neighbours without a cloud.

## σ innovation (what σ adds here)

> **FedAvg weighted by σ.** A device whose update is uncertain
> does not pull the global model. v294 names the four predicates
> that make federated learning survivable: accept-polarity,
> DP-regime monotonicity, non-IID routing, and a trust-only mesh.

## v0 manifests

Enumerated in [`src/v294/federated.h`](../../src/v294/federated.h);
pinned by
[`benchmarks/v294/check_v294_federated_sigma_gated.sh`](../../benchmarks/v294/check_v294_federated_sigma_gated.sh).

### 1. Gated aggregation (exactly 3 canonical devices)

| id         | σ_device | weight | accepted |
|------------|----------|--------|----------|
| `device_a` | 0.10     | 0.60   | `true`   |
| `device_b` | 0.30     | 0.40   | `true`   |
| `device_c` | 0.80     | 0.00   | `false`  |

Contract: `accepted iff σ_device ≤ τ_device = 0.40` (both branches
fire); accepted weights sum to 1.0 ± 1e-3; weights strictly
decreasing with σ across ACCEPTED rows.

### 2. Privacy + σ (exactly 3 canonical DP regimes)

| regime           | ε    | σ_after_noise | classification    |
|------------------|------|---------------|-------------------|
| `too_low_noise`  | 10.0 | 0.05          | PRIVACY_RISK      |
| `optimal_noise`  |  1.0 | 0.20          | OPTIMAL           |
| `too_high_noise` |  0.1 | 0.75          | SIGNAL_DESTROYED  |

σ strictly monotonically increasing as ε strictly monotonically
decreasing — more DP noise → higher σ; exactly 1 OPTIMAL row.

### 3. Non-IID σ detection (exactly 3 canonical rows)

| device_type          | σ_dist | strategy       |
|----------------------|--------|----------------|
| `similar_data`       | 0.10   | GLOBAL_MODEL   |
| `slightly_different` | 0.40   | HYBRID         |
| `very_different`     | 0.75   | PERSONALIZED   |

`GLOBAL_MODEL iff σ_dist < 0.20`, `PERSONALIZED iff σ_dist > 0.60`,
`HYBRID` otherwise — all three branches fire.

### 4. Sovereign federated mesh (exactly 3 canonical edges)

| edge   | σ_neighbor | trusted |
|--------|------------|---------|
| `a->b` | 0.10       | `true`  |
| `b->c` | 0.25       | `true`  |
| `a->z` | 0.70       | `false` |

`trusted iff σ_neighbor ≤ τ_mesh = 0.30` (both branches fire);
`central_server = false`; `single_point_of_failure = false`.

### σ_fed (surface hygiene)

```
σ_fed = 1 −
  (dev_rows_ok + dev_accept_polarity_ok +
   dev_weight_sum_ok + dev_weight_mono_ok +
   dp_rows_ok + dp_order_ok + dp_optimal_ok +
   niid_rows_ok + niid_order_ok + niid_classify_ok +
   mesh_rows_ok + mesh_polarity_ok + mesh_no_server_ok) /
  (3 + 1 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1)
```

v0 requires `σ_fed == 0.0`.

## Merge-gate contracts

- 3 canonical devices; accept polarity holds; accepted weights
  sum to 1.0; weights strictly decrease with σ.
- 3 DP regimes; σ strictly increasing as ε strictly decreasing;
  exactly 1 OPTIMAL.
- 3 non-IID rows; σ_dist strictly increasing; three routing
  branches fire.
- 3 mesh edges; trust polarity holds; no central server.
- `σ_fed ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** names the aggregation / privacy / non-IID /
  mesh contracts as predicates.
- **v294.1 (named, not implemented)** is real FedAvg over a
  WireGuard/mTLS mesh, a live DP-ε controller that tunes noise
  from observed σ, and a gossip-protocol rollout for per-device
  personalised heads.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).
