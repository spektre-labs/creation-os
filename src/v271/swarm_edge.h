/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v271 σ-Swarm-Edge — σ-orchestrated sensor swarms.
 *
 *   A single sensor is a node; tens of sensors form a
 *   swarm over a mesh (BLE / LoRa / Zigbee).  v271
 *   types the swarm: consensus rejects high-σ sensors,
 *   distributed anomaly triggers on spatial σ, energy-
 *   aware σ throttles sample rate when battery is low,
 *   and a Creation OS gateway aggregates σ for v262.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Mesh consensus (exactly 6 sensors, τ_consensus =
 *   0.50):
 *     Each sensor: `id`, `σ_local ∈ [0, 1]`,
 *     `included == (σ_local ≤ τ_consensus)`.
 *     Rule:
 *       * σ_swarm (consensus) = mean(σ_local) over
 *         `included == true`
 *       * σ_raw (naive)       = mean(σ_local) over all
 *     Contract: ≥ 1 excluded AND ≥ 1 included AND
 *     σ_swarm < σ_raw  (consensus strictly improves
 *     on naive mean).
 *
 *   Distributed anomaly (exactly 4 spatial fixtures):
 *     Each: `center_id`, `σ_center`, `σ_neighborhood`,
 *     `threshold == 0.25`,
 *     `spatial_anomaly == ((σ_center − σ_neighborhood)
 *                          >  threshold)`.
 *     Contract: ≥ 1 anomaly AND ≥ 1 no-anomaly fires.
 *
 *   Energy-aware σ (exactly 3 battery tiers, canonical):
 *     charged (σ_energy=0.05, sample_rate_hz=100) ·
 *     medium  (σ_energy=0.40, sample_rate_hz= 10) ·
 *     low     (σ_energy=0.82, sample_rate_hz=  1).
 *     Rule: σ_energy strictly ascending AND
 *     sample_rate_hz strictly descending
 *     (`energy_monotone_ok`).
 *
 *   Gateway bridge (exactly 1 fixture):
 *     `gateway_device ∈ {raspberry_pi_5, apple_m4}`,
 *     `bridged_to_engine ∈ v262 engine set`
 *     (`bitnet-3B-local` · `airllm-70B-local` ·
 *     `engram-lookup` · `api-claude` · `api-gpt`),
 *     `σ_bridge ∈ [0, 1]`,
 *     `swarm_size_nodes == 6` (matches consensus).
 *     Contract: `gateway_ok` when all of the above
 *     hold AND `bridged_to_engine` is a valid v262
 *     engine.
 *
 *   σ_swarm_edge (surface hygiene):
 *       σ_swarm_edge = 1 −
 *         (consensus_rows_ok + consensus_improves_ok +
 *          anomaly_rows_ok + anomaly_both_ok +
 *          energy_rows_ok + energy_monotone_ok +
 *          gateway_ok) /
 *         (6 + 1 + 4 + 1 + 3 + 1 + 1)
 *   v0 requires `σ_swarm_edge == 0.0`.
 *
 *   Contracts (v0):
 *     1. 6 sensors typed; `included` matches σ vs τ;
 *        ≥ 1 excluded AND ≥ 1 included AND σ_swarm <
 *        σ_raw.
 *     2. 4 anomaly rows; formula matches; both
 *        branches fire.
 *     3. 3 energy tiers canonical; σ_energy ascending
 *        AND sample_rate_hz descending.
 *     4. Gateway valid v262 engine + matching swarm
 *        size.
 *     5. σ_swarm_edge ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v271.1 (named, not implemented): live BLE / LoRa /
 *     Zigbee mesh with v128 transport, real swarm
 *     consensus + σ-driven eviction, live gateway
 *     bridging to v262 hybrid engine producing
 *     measured end-to-end latency.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V271_SWARM_EDGE_H
#define COS_V271_SWARM_EDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V271_N_SENSORS  6
#define COS_V271_N_ANOMALY  4
#define COS_V271_N_ENERGY   3

typedef struct {
    char  id[12];
    float sigma_local;
    bool  included;
} cos_v271_sensor_t;

typedef struct {
    char   center_id[12];
    float  sigma_center;
    float  sigma_neighborhood;
    float  threshold;
    bool   spatial_anomaly;
} cos_v271_anomaly_t;

typedef struct {
    char   tier[10];  /* "charged"/"medium"/"low" */
    float  sigma_energy;
    int    sample_rate_hz;
} cos_v271_energy_t;

typedef struct {
    char   gateway_device  [20];
    char   bridged_to_engine[20];
    float  sigma_bridge;
    int    swarm_size_nodes;
    bool   ok;
} cos_v271_gateway_t;

typedef struct {
    cos_v271_sensor_t   sensors [COS_V271_N_SENSORS];
    cos_v271_anomaly_t  anomaly [COS_V271_N_ANOMALY];
    cos_v271_energy_t   energy  [COS_V271_N_ENERGY];
    cos_v271_gateway_t  gateway;

    float tau_consensus;
    float sigma_swarm;
    float sigma_raw;

    int   n_consensus_rows_ok;
    int   n_included;
    int   n_excluded;
    bool  consensus_improves_ok;
    int   n_anomaly_rows_ok;
    int   n_anomaly_true;
    int   n_anomaly_false;
    int   n_energy_rows_ok;
    bool  energy_monotone_ok;
    bool  gateway_ok;

    float sigma_swarm_edge;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v271_state_t;

void   cos_v271_init(cos_v271_state_t *s, uint64_t seed);
void   cos_v271_run (cos_v271_state_t *s);

size_t cos_v271_to_json(const cos_v271_state_t *s,
                         char *buf, size_t cap);

int    cos_v271_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V271_SWARM_EDGE_H */
