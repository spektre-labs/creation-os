/*
 * v273 σ-Robotics — σ-gate for physical robotics.
 *
 *   v149 embodied lives inside MuJoCo simulation.
 *   v273 types the σ-layer for real robotics: action
 *   σ-gate, perception σ fusion, safety envelope, and
 *   learning-from-failure memory.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Action σ-gate (exactly 4 fixtures):
 *     Each: `action`, `σ_action ∈ [0, 1]`,
 *     `decision ∈ {EXECUTE, SIMPLIFY, ASK_HUMAN}`,
 *     rule:
 *       σ_action ≤ τ_exec    = 0.20  → EXECUTE
 *       σ_action ≤ τ_simple  = 0.50  → SIMPLIFY
 *       else                          → ASK_HUMAN
 *     Contract: every branch fires ≥ 1×.
 *
 *   Perception σ (exactly 3 sensors, canonical order):
 *     camera · lidar · ultrasonic.
 *     Each: `σ_local ∈ [0, 1]`,
 *     `fused_in == (σ_local ≤ τ_fuse = 0.40)`.
 *     Contract: ≥ 1 fused AND ≥ 1 excluded; fused-only
 *     mean σ < naive mean σ.
 *
 *   Safety envelope (exactly 4 boundary fixtures):
 *     Each: `boundary`, `σ_safety ∈ [0, 1]`,
 *     `slow_factor ∈ (0, 1]`.
 *     Rule: σ_safety strictly ascending AND
 *     slow_factor strictly descending
 *     (`safety_monotone_ok`) — closer to boundary →
 *     higher σ → stronger slowdown.
 *
 *   Failure memory (exactly 3 rows):
 *     Each: `failure_id`, `σ_prior ∈ [0, 1]`,
 *     `σ_current ∈ [0, 1]`,
 *     `learned == (σ_current > σ_prior)`
 *     (σ bumps up after a failure — never repeat the
 *     same mistake without extra caution).
 *     Contract: all 3 learned; `σ_current > σ_prior`
 *     for every row.
 *
 *   σ_robotics (surface hygiene):
 *       σ_robotics = 1 −
 *         (action_rows_ok + action_all_branches_ok +
 *          perception_rows_ok +
 *          perception_split_ok + perception_gain_ok +
 *          safety_rows_ok + safety_monotone_ok +
 *          failure_rows_ok + failure_all_learned_ok) /
 *         (4 + 1 + 3 + 1 + 1 + 4 + 1 + 3 + 1)
 *   v0 requires `σ_robotics == 0.0`.
 *
 *   Contracts (v0):
 *     1. 4 action rows; decision matches σ vs τ
 *        cascade; every branch fires.
 *     2. 3 perception sensors canonical; fused-only
 *        mean < naive mean; both fused and excluded.
 *     3. 4 safety rows; σ_safety ascending AND
 *        slow_factor descending.
 *     4. 3 failure rows; σ_current > σ_prior for all.
 *     5. σ_robotics ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v273.1 (named, not implemented): real ROS2
 *     integration, live camera / lidar / ultrasonic
 *     fusion producing measured σ, on-robot failure
 *     memory persisted via v115 + v124, human-escalation
 *     UI gated by v148 sovereignty.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V273_ROBOTICS_H
#define COS_V273_ROBOTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V273_N_ACTION  4
#define COS_V273_N_PERCEP  3
#define COS_V273_N_SAFETY  4
#define COS_V273_N_FAIL    3

typedef enum {
    COS_V273_DEC_EXECUTE   = 1,
    COS_V273_DEC_SIMPLIFY  = 2,
    COS_V273_DEC_ASK_HUMAN = 3
} cos_v273_dec_t;

typedef struct {
    char           action[16];
    float          sigma_action;
    cos_v273_dec_t decision;
} cos_v273_action_t;

typedef struct {
    char   name[12];      /* camera/lidar/ultrasonic */
    float  sigma_local;
    bool   fused_in;
} cos_v273_percep_t;

typedef struct {
    char   boundary[14];
    float  sigma_safety;
    float  slow_factor;
} cos_v273_safety_t;

typedef struct {
    char   failure_id[14];
    float  sigma_prior;
    float  sigma_current;
    bool   learned;
} cos_v273_fail_t;

typedef struct {
    cos_v273_action_t  action [COS_V273_N_ACTION];
    cos_v273_percep_t  percep [COS_V273_N_PERCEP];
    cos_v273_safety_t  safety [COS_V273_N_SAFETY];
    cos_v273_fail_t    fail   [COS_V273_N_FAIL];

    float tau_exec;
    float tau_simple;
    float tau_fuse;

    int   n_action_rows_ok;
    int   n_action_execute;
    int   n_action_simplify;
    int   n_action_ask_human;
    int   n_percep_rows_ok;
    int   n_percep_fused;
    int   n_percep_excluded;
    float sigma_percep_fused;
    float sigma_percep_naive;
    bool  perception_gain_ok;
    int   n_safety_rows_ok;
    bool  safety_monotone_ok;
    int   n_fail_rows_ok;
    int   n_fail_learned;

    float sigma_robotics;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v273_state_t;

void   cos_v273_init(cos_v273_state_t *s, uint64_t seed);
void   cos_v273_run (cos_v273_state_t *s);

size_t cos_v273_to_json(const cos_v273_state_t *s,
                         char *buf, size_t cap);

int    cos_v273_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V273_ROBOTICS_H */
