/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v294 σ-Federated — σ-gated federated learning.
 *
 *   Federated learning trains a shared model across
 *   many devices without moving the data.  v294 types
 *   the σ discipline that keeps the aggregation
 *   honest, the privacy budget legible, and the mesh
 *   survivable.  Four v0 manifests: aggregation
 *   weights follow σ, DP regimes classify cleanly,
 *   non-IID detection routes per device, and the
 *   sovereign mesh rejects untrusted neighbours.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Gated aggregation (exactly 3 canonical devices):
 *     `device_a` σ=0.10 accepted weight=0.60;
 *     `device_b` σ=0.30 accepted weight=0.40;
 *     `device_c` σ=0.80 REJECTED weight=0.00.
 *     Contract: `accepted iff σ_device ≤ τ_device =
 *     0.40` (both branches fire); weights of ACCEPTED
 *     devices sum to 1.0 ± 1e-3; weights strictly
 *     decreasing with σ across ACCEPTED rows — low
 *     σ pulls more of the aggregate, high σ is
 *     quarantined.
 *
 *   Privacy + σ (exactly 3 canonical DP regimes):
 *     `too_low_noise`   ε=10.0 σ=0.05 PRIVACY_RISK;
 *     `optimal_noise`   ε= 1.0 σ=0.20 OPTIMAL;
 *     `too_high_noise`  ε= 0.1 σ=0.75 SIGNAL_DESTROYED.
 *     Contract: 3 DISTINCT regimes in order; σ
 *     strictly monotonically increasing as ε strictly
 *     decreasing (more DP noise → higher σ);
 *     exactly 1 OPTIMAL row and exactly 2 non-optimal
 *     rows — σ names the Goldilocks point, not a
 *     knob.
 *
 *   Non-IID σ detection (exactly 3 canonical routing rows):
 *     `similar_data`       σ_dist=0.10 → GLOBAL_MODEL;
 *     `slightly_different` σ_dist=0.40 → HYBRID;
 *     `very_different`     σ_dist=0.75 → PERSONALIZED.
 *     Contract: σ_dist strictly monotonically
 *     increasing; `GLOBAL_MODEL iff σ_dist <
 *     δ_global=0.20`, `PERSONALIZED iff σ_dist >
 *     δ_personal=0.60`, `HYBRID` otherwise — all
 *     three branches fire.
 *
 *   Sovereign federated mesh (exactly 3 canonical edges):
 *     `a->b` σ_neighbor=0.10 trusted;
 *     `b->c` σ_neighbor=0.25 trusted;
 *     `a->z` σ_neighbor=0.70 REJECTED.
 *     Contract: `trusted iff σ_neighbor ≤ τ_mesh =
 *     0.30` (both branches fire); `central_server ==
 *     false`; `single_point_of_failure == false` —
 *     the mesh learns from its trusted neighbours
 *     without a cloud.
 *
 *   σ_fed (surface hygiene):
 *       σ_fed = 1 −
 *         (dev_rows_ok + dev_accept_polarity_ok +
 *          dev_weight_sum_ok + dev_weight_mono_ok +
 *          dp_rows_ok + dp_order_ok + dp_optimal_ok +
 *          niid_rows_ok + niid_order_ok +
 *          niid_classify_ok +
 *          mesh_rows_ok + mesh_polarity_ok +
 *          mesh_no_server_ok) /
 *         (3 + 1 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1 +
 *          3 + 1 + 1)
 *   v0 requires `σ_fed == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 canonical devices; accept polarity holds;
 *        accepted weights sum to 1.0; weights
 *        strictly decrease with σ.
 *     2. 3 DP regimes; σ strictly increasing as ε
 *        strictly decreasing; exactly 1 OPTIMAL.
 *     3. 3 non-IID rows; σ_dist strictly increasing;
 *        three routing branches fire.
 *     4. 3 mesh edges; trust polarity holds; no
 *        central server, no single point of failure.
 *     5. σ_fed ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v294.1 (named, not implemented): real FedAvg
 *     over a WireGuard/mTLS mesh, a live DP-ε
 *     controller that tunes noise from observed σ,
 *     and a gossip-protocol rollout for per-device
 *     personalised heads.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V294_FEDERATED_H
#define COS_V294_FEDERATED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V294_N_DEV   3
#define COS_V294_N_DP    3
#define COS_V294_N_NIID  3
#define COS_V294_N_MESH  3

#define COS_V294_TAU_DEVICE    0.40f
#define COS_V294_DELTA_GLOBAL  0.20f
#define COS_V294_DELTA_PERSONAL 0.60f
#define COS_V294_TAU_MESH      0.30f

typedef struct {
    char  id[12];
    float sigma_device;
    float weight;
    bool  accepted;
} cos_v294_dev_t;

typedef struct {
    char  regime[18];
    float dp_epsilon;
    float sigma_after_noise;
    char  classification[20];
} cos_v294_dp_t;

typedef struct {
    char  device_type[22];
    float sigma_distribution;
    char  strategy[16];
} cos_v294_niid_t;

typedef struct {
    char  edge[6];
    float sigma_neighbor;
    bool  trusted;
} cos_v294_mesh_t;

typedef struct {
    cos_v294_dev_t   dev  [COS_V294_N_DEV];
    cos_v294_dp_t    dp   [COS_V294_N_DP];
    cos_v294_niid_t  niid [COS_V294_N_NIID];
    cos_v294_mesh_t  mesh [COS_V294_N_MESH];

    int   n_dev_rows_ok;
    bool  dev_accept_polarity_ok;
    bool  dev_weight_sum_ok;
    bool  dev_weight_mono_ok;

    int   n_dp_rows_ok;
    bool  dp_order_ok;
    bool  dp_optimal_ok;

    int   n_niid_rows_ok;
    bool  niid_order_ok;
    bool  niid_classify_ok;

    int   n_mesh_rows_ok;
    bool  mesh_polarity_ok;
    bool  mesh_no_server_ok;

    float sigma_fed;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v294_state_t;

void   cos_v294_init(cos_v294_state_t *s, uint64_t seed);
void   cos_v294_run (cos_v294_state_t *s);

size_t cos_v294_to_json(const cos_v294_state_t *s,
                         char *buf, size_t cap);

int    cos_v294_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V294_FEDERATED_H */
