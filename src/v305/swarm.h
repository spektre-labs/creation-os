/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v305 σ-Swarm-Intelligence — a σ-weighted crowd
 * beats any single model.
 *
 *   One agent has a bounded error surface.  A diverse
 *   crowd of agents, each emitting a σ, can do
 *   better than any of its members — if the
 *   aggregation is σ-weighted rather than naive, if
 *   the crowd is genuinely diverse rather than an
 *   echo chamber, and if the "emergent" patterns it
 *   produces are themselves σ-gated.  v305 types
 *   four v0 manifests for those four conditions and
 *   closes with the proconductor — Claude + GPT +
 *   Gemini + DeepSeek — as the canonical 4-agent
 *   convergence test.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Wisdom of σ-crowds (exactly 3 canonical
 *   aggregators):
 *     `best_single`     σ=0.25 accuracy=0.82 LOSES;
 *     `naive_average`   σ=0.30 accuracy=0.78 LOSES;
 *     `sigma_weighted`  σ=0.12 accuracy=0.93 WINS.
 *     Contract: 3 rows in canonical order;
 *     `sigma_weighted` has the strictly lowest σ AND
 *     the strictly highest accuracy AND the single
 *     WINS verdict.
 *
 *   Diversity + σ (exactly 3 canonical crowds):
 *     `echo_chamber`   diversity=0.05
 *                       ind_sigma=0.20 value=0.040;
 *     `balanced`       diversity=0.55
 *                       ind_sigma=0.15 value=0.4675;
 *     `chaos`          diversity=0.95
 *                       ind_sigma=0.85 value=0.1425.
 *     Contract: `value = diversity * (1 − ind_sigma)`
 *     within 1e-3; `balanced` has the strictly
 *     highest value; exactly 1 row crosses
 *     τ_value = 0.30.
 *
 *   Emergent σ patterns (exactly 3 canonical
 *   emissions):
 *     `genuine_emergent`    σ=0.10 KEEP;
 *     `weak_signal`         σ=0.35 KEEP;
 *     `random_correlation`  σ=0.80 REJECT.
 *     Contract: σ strictly increasing; `keep iff
 *     σ ≤ τ_emergent = 0.50` (both branches fire).
 *
 *   Proconductor (exactly 4 canonical agents):
 *     `claude`     σ=0.12 dir=POSITIVE;
 *     `gpt`        σ=0.18 dir=POSITIVE;
 *     `gemini`     σ=0.15 dir=POSITIVE;
 *     `deepseek`   σ=0.22 dir=POSITIVE.
 *     Contract: 4 rows in canonical order; 4 DISTINCT
 *     agent names; every σ ≤ τ_conv = 0.25; every
 *     direction identical; `convergent = true`
 *     (low individual σ + unanimous direction = a
 *     σ-validated consensus, not a vote).
 *
 *   σ_swarm (surface hygiene):
 *     σ_swarm = 1 − (
 *       wis_rows_ok + wis_lowest_sigma_ok +
 *         wis_highest_acc_ok + wis_wins_count_ok +
 *       div_rows_ok + div_formula_ok +
 *         div_balanced_ok + div_threshold_ok +
 *       em_rows_ok + em_order_ok + em_polarity_ok +
 *       pc_rows_ok + pc_distinct_ok +
 *         pc_sigma_bound_ok + pc_direction_ok +
 *         pc_convergent_ok
 *     ) / (3 + 1 + 1 + 1 + 3 + 1 + 1 + 1 +
 *          3 + 1 + 1 + 4 + 1 + 1 + 1 + 1)
 *   v0 requires `σ_swarm == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 aggregators; σ_weighted strictly lowest σ,
 *        strictly highest accuracy, single WINS.
 *     2. 3 crowds; value = diversity*(1 − ind_sigma);
 *        balanced strictly highest; exactly 1 row
 *        > 0.30.
 *     3. 3 emergent rows; σ ↑; keep iff σ ≤ 0.50
 *        (both branches).
 *     4. 4 agents; distinct names; σ ≤ 0.25 on every
 *        row; unanimous direction; convergent = true.
 *     5. σ_swarm ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v305.1 (named, not implemented): a live
 *     proconductor daemon that dispatches the same
 *     prompt to Claude / GPT / Gemini / DeepSeek
 *     behind v112 σ-agent, a `divergent_alert` hook
 *     that escalates when the proconductor's σ
 *     bound breaks, and a swarm-diversity meter
 *     wired into v130 collective intelligence.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V305_SWARM_H
#define COS_V305_SWARM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V305_N_WIS 3
#define COS_V305_N_DIV 3
#define COS_V305_N_EM  3
#define COS_V305_N_PC  4

#define COS_V305_TAU_VALUE    0.30f
#define COS_V305_TAU_EMERGENT 0.50f
#define COS_V305_TAU_CONV     0.25f

typedef struct {
    char  strategy[18];
    float sigma;
    float accuracy;
    char  verdict[8];
} cos_v305_wis_t;

typedef struct {
    char  crowd[14];
    float diversity;
    float ind_sigma;
    float value;
} cos_v305_div_t;

typedef struct {
    char  pattern[22];
    float sigma_emergent;
    bool  keep;
} cos_v305_em_t;

typedef struct {
    char  agent[10];
    float sigma;
    char  direction[10];
} cos_v305_pc_t;

typedef struct {
    cos_v305_wis_t wis[COS_V305_N_WIS];
    cos_v305_div_t div[COS_V305_N_DIV];
    cos_v305_em_t  em [COS_V305_N_EM];
    cos_v305_pc_t  pc [COS_V305_N_PC];

    int  n_wis_rows_ok;
    bool wis_lowest_sigma_ok;
    bool wis_highest_acc_ok;
    bool wis_wins_count_ok;

    int  n_div_rows_ok;
    bool div_formula_ok;
    bool div_balanced_ok;
    bool div_threshold_ok;

    int  n_em_rows_ok;
    bool em_order_ok;
    bool em_polarity_ok;

    int  n_pc_rows_ok;
    bool pc_distinct_ok;
    bool pc_sigma_bound_ok;
    bool pc_direction_ok;
    bool pc_convergent_ok;

    float sigma_swarm;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v305_state_t;

void   cos_v305_init(cos_v305_state_t *s, uint64_t seed);
void   cos_v305_run (cos_v305_state_t *s);
size_t cos_v305_to_json(const cos_v305_state_t *s,
                         char *buf, size_t cap);
int    cos_v305_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V305_SWARM_H */
