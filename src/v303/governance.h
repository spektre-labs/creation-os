/*
 * v303 σ-Governance — σ on organisations.
 *
 *   v199..v203 introduced the social layer.  v303
 *   walks the same discipline into a company: a
 *   decision has a σ, a meeting has a σ, a message
 *   has a σ, and the institution as a whole has a
 *   coherence `K(t) = ρ · I_φ · F` that must stay
 *   above `K_crit = 0.127` or collapse is imminent.
 *   v303 is the manifest.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Decision σ (exactly 3 canonical decisions):
 *     `strategy_matches_ops`      σ=0.05 COHERENT;
 *     `strategy_partially_realised` σ=0.35 DEGRADED;
 *     `strategy_ignored`          σ=0.75 INCOHERENT.
 *     Contract: σ strictly increasing; 3 DISTINCT
 *     verdicts.
 *
 *   Meeting σ (exactly 3 canonical meetings):
 *     `meeting_perfect`     10 made · 10 realised
 *                           σ_meeting=0.00;
 *     `meeting_quarter`      8 made ·  6 realised
 *                           σ_meeting=0.25;
 *     `meeting_noise`       10 made ·  3 realised
 *                           σ_meeting=0.70.
 *     Contract: σ_meeting == 1 − realised/made
 *     within 1e-3; σ strictly increasing.
 *
 *   Communication σ (exactly 3 canonical channels):
 *     `clear_message`   σ_comm=0.10 CLEAR;
 *     `slightly_vague`  σ_comm=0.35 CLEAR;
 *     `highly_vague`    σ_comm=0.80 AMBIGUOUS.
 *     Contract: σ strictly increasing;
 *     `clear iff σ_comm ≤ τ_comm = 0.50` (both
 *     branches fire).
 *
 *   Institutional K(t) (exactly 3 canonical institutions):
 *     `healthy_org`      ρ=0.85 I_φ=0.60 F=0.70
 *                         K≈0.357  VIABLE;
 *     `warning_org`      ρ=0.50 I_φ=0.50 F=0.60
 *                         K≈0.150  WARNING;
 *     `collapsing_org`   ρ=0.30 I_φ=0.40 F=0.50
 *                         K≈0.060  COLLAPSE.
 *     Contract: K == ρ·I_φ·F within 1e-3; VIABLE iff
 *     K ≥ K_warn = 0.20; COLLAPSE iff K < K_crit =
 *     0.127; WARNING iff K_crit ≤ K < K_warn; all
 *     three branches fire.
 *
 *   σ_gov (surface hygiene):
 *     σ_gov = 1 − (
 *       dec_rows_ok + dec_order_ok + dec_distinct_ok +
 *       mtg_rows_ok + mtg_formula_ok + mtg_order_ok +
 *       comm_rows_ok + comm_order_ok + comm_polarity_ok +
 *       kt_rows_ok + kt_formula_ok + kt_polarity_ok
 *     ) / (3 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1)
 *   v0 requires `σ_gov == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 decisions; σ ↑; 3 distinct verdicts.
 *     2. 3 meetings; σ_meeting = 1 − realised/made;
 *        σ ↑.
 *     3. 3 channels; σ ↑; clear iff σ ≤ 0.50 (both
 *        branches).
 *     4. 3 institutions; K = ρ·I_φ·F; VIABLE /
 *        WARNING / COLLAPSE firing on K_warn and
 *        K_crit.
 *     5. σ_gov ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v303.1 (named, not implemented): live OKR / KPI
 *     pipes feeding decision-σ and meeting-σ
 *     straight into the v244 observability plane,
 *     and an institutional-K dashboard that raises a
 *     v283 constitutional-AI escalation when K
 *     crosses K_warn.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V303_GOVERNANCE_H
#define COS_V303_GOVERNANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V303_N_DEC  3
#define COS_V303_N_MTG  3
#define COS_V303_N_COMM 3
#define COS_V303_N_KT   3

#define COS_V303_TAU_COMM 0.50f
#define COS_V303_K_CRIT   0.127f
#define COS_V303_K_WARN   0.20f

typedef struct {
    char  label[28];
    float sigma_decision;
    char  verdict[12];
} cos_v303_dec_t;

typedef struct {
    char  label[20];
    int   decisions_made;
    int   decisions_realised;
    float sigma_meeting;
} cos_v303_mtg_t;

typedef struct {
    char  channel[16];
    float sigma_comm;
    bool  clear;
} cos_v303_comm_t;

typedef struct {
    char  org[16];
    float rho;
    float i_phi;
    float f;
    float k;
    char  status[10];
} cos_v303_kt_t;

typedef struct {
    cos_v303_dec_t  dec [COS_V303_N_DEC];
    cos_v303_mtg_t  mtg [COS_V303_N_MTG];
    cos_v303_comm_t comm[COS_V303_N_COMM];
    cos_v303_kt_t   kt  [COS_V303_N_KT];

    int  n_dec_rows_ok;
    bool dec_order_ok;
    bool dec_distinct_ok;

    int  n_mtg_rows_ok;
    bool mtg_formula_ok;
    bool mtg_order_ok;

    int  n_comm_rows_ok;
    bool comm_order_ok;
    bool comm_polarity_ok;

    int  n_kt_rows_ok;
    bool kt_formula_ok;
    bool kt_polarity_ok;

    float sigma_gov;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v303_state_t;

void   cos_v303_init(cos_v303_state_t *s, uint64_t seed);
void   cos_v303_run (cos_v303_state_t *s);
size_t cos_v303_to_json(const cos_v303_state_t *s,
                         char *buf, size_t cap);
int    cos_v303_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V303_GOVERNANCE_H */
