/*
 * v277 σ-Distill-Runtime — live teacher/student
 *                           distillation with σ-filter.
 *
 *   An API model (Claude / GPT) teaches a local
 *   BitNet student.  σ decides what is worth learning
 *   and which domain is "covered" well enough to avoid
 *   the API.  Over time the API share falls and the
 *   sovereign stack absorbs more queries.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Pair manifest (exactly 1 fixture):
 *     `teacher = api-claude` and
 *     `student = bitnet-3B-local`, both strings.
 *     Contract: both non-empty AND distinct.
 *
 *   σ-filtered distillation (exactly 4 fixtures,
 *   τ_learn = 0.25):
 *     Each: `query_id`, `σ_teacher ∈ [0, 1]`,
 *     `decision ∈ {LEARN, SKIP}`,
 *     rule: `σ_teacher ≤ τ_learn → LEARN` else SKIP.
 *     Contract: ≥ 1 LEARN AND ≥ 1 SKIP.
 *
 *   Domain adaptation (exactly 3 fixtures, canonical
 *   order, τ_domain = 0.30):
 *     `law`, `code`, `medical`, each with `σ_domain`,
 *     `route ∈ {LOCAL_ONLY, API}`,
 *     rule: `σ_domain ≤ τ_domain → LOCAL_ONLY` else
 *     API.
 *     Contract: ≥ 1 LOCAL_ONLY AND ≥ 1 API.
 *
 *   Sovereign trajectory (exactly 4 checkpoints,
 *   canonical order):
 *     `month_0`, `month_1`, `month_3`, `month_12`,
 *     each with `api_share ∈ [0, 1]`,
 *     `local_share ∈ [0, 1]`, `cost_eur_per_month ≥ 0`.
 *     Contract:
 *       - for each checkpoint: api_share + local_share
 *         ≈ 1.0 (± 1e-4)
 *       - api_share strictly decreasing
 *       - local_share strictly increasing
 *       - cost strictly decreasing
 *       - first checkpoint api_share ≥ 0.75
 *       - last  checkpoint api_share ≤ 0.10
 *
 *   σ_distill (surface hygiene):
 *       σ_distill = 1 −
 *         (pair_ok +
 *          filter_rows_ok + filter_both_ok +
 *          domain_rows_ok + domain_both_ok +
 *          trajectory_rows_ok + traj_shares_ok +
 *          traj_monotone_api_ok +
 *          traj_monotone_local_ok +
 *          traj_monotone_cost_ok +
 *          traj_anchors_ok) /
 *         (1 + 4 + 1 + 3 + 1 + 4 + 1 + 1 + 1 + 1 + 1)
 *   v0 requires `σ_distill == 0.0`.
 *
 *   Contracts (v0):
 *     1. Pair typed: teacher + student distinct.
 *     2. 4 filter rows; LEARN iff σ ≤ 0.25; both
 *        branches fire.
 *     3. 3 domains canonical; LOCAL_ONLY iff σ ≤ 0.30;
 *        both branches fire.
 *     4. 4 trajectory checkpoints with monotone api↓,
 *        local↑, cost↓; anchors pass.
 *     5. σ_distill ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v277.1 (named, not implemented): live teacher/
 *     student loop wired into v262, real σ-filter on
 *     every teacher trace, domain router reading live
 *     σ_domain, measured monthly API share on a user's
 *     workload.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V277_DISTILL_RUNTIME_H
#define COS_V277_DISTILL_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V277_N_FILTER     4
#define COS_V277_N_DOMAINS    3
#define COS_V277_N_CHECKPTS   4

typedef enum {
    COS_V277_FILT_LEARN = 1,
    COS_V277_FILT_SKIP  = 2
} cos_v277_filt_t;

typedef enum {
    COS_V277_ROUTE_LOCAL_ONLY = 1,
    COS_V277_ROUTE_API        = 2
} cos_v277_route_t;

typedef struct {
    char teacher[20];
    char student[24];
} cos_v277_pair_t;

typedef struct {
    int             query_id;
    float           sigma_teacher;
    cos_v277_filt_t decision;
} cos_v277_filter_t;

typedef struct {
    char              name[10];
    float             sigma_domain;
    cos_v277_route_t  route;
} cos_v277_domain_t;

typedef struct {
    char  label[10];
    float api_share;
    float local_share;
    float cost_eur_per_month;
} cos_v277_checkpt_t;

typedef struct {
    cos_v277_pair_t     pair;
    cos_v277_filter_t   filter  [COS_V277_N_FILTER];
    cos_v277_domain_t   domains [COS_V277_N_DOMAINS];
    cos_v277_checkpt_t  traj    [COS_V277_N_CHECKPTS];

    float tau_learn;   /* 0.25 */
    float tau_domain;  /* 0.30 */

    bool  pair_ok;

    int   n_filter_rows_ok;
    int   n_filter_learn;
    int   n_filter_skip;

    int   n_domain_rows_ok;
    int   n_domain_local;
    int   n_domain_api;

    int   n_trajectory_rows_ok;
    bool  traj_shares_ok;
    bool  traj_monotone_api_ok;
    bool  traj_monotone_local_ok;
    bool  traj_monotone_cost_ok;
    bool  traj_anchors_ok;

    float sigma_distill;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v277_state_t;

void   cos_v277_init(cos_v277_state_t *s, uint64_t seed);
void   cos_v277_run (cos_v277_state_t *s);

size_t cos_v277_to_json(const cos_v277_state_t *s,
                         char *buf, size_t cap);

int    cos_v277_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V277_DISTILL_RUNTIME_H */
