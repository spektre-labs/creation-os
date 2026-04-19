/*
 * v280 σ-MoE — Mixture of Experts with σ on the router.
 *
 *   MoE routes each token to a sparse subset of experts
 *   (often top-1 or top-2 of N).  The router itself is a
 *   uncertainty problem: how confident is the top-k
 *   decision?  v280 types the σ-layer on top of MoE as
 *   four merge-gate manifests covering the routing gate,
 *   task routing signatures, speculative prefetch, and
 *   MoBiE-style adaptive quantisation.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Routing gate (exactly 4 fixtures, τ_route = 0.35):
 *     Each: `token_id`, `σ_routing ∈ [0, 1]`,
 *     `decision ∈ {TOP_K, DIVERSIFY}`,
 *     rule: `σ_routing ≤ τ_route → TOP_K` else
 *     `DIVERSIFY` (activate more experts).
 *     Contract: ≥ 1 TOP_K AND ≥ 1 DIVERSIFY.
 *
 *   Routing signatures (exactly 3 tasks, canonical
 *   order `code`, `math`, `creative`, τ_sig = 0.40):
 *     Each: `task`, `routing_entropy ∈ [0, 1]`,
 *     `familiar ∈ {KNOWN, NOVEL}`,
 *     rule: `routing_entropy ≤ τ_sig → KNOWN` else
 *     `NOVEL`.
 *     Contract: ≥ 1 KNOWN AND ≥ 1 NOVEL.
 *
 *   Prefetch (exactly 3 fixtures, canonical order
 *   `low`, `mid`, `high`, τ_prefetch_low = 0.20,
 *   τ_prefetch_mid = 0.50):
 *     Each: `label`, `σ_prefetch ∈ [0, 1]`,
 *     `strategy ∈ {AGGRESSIVE, BALANCED, CONSERVATIVE}`,
 *     cascade:
 *       σ_prefetch ≤ 0.20 → AGGRESSIVE
 *       σ_prefetch ≤ 0.50 → BALANCED
 *       else              → CONSERVATIVE.
 *     Contract: every branch fires exactly once.
 *
 *   MoBiE shift (exactly 3 experts, canonical order
 *   `expert_0`, `expert_1`, `expert_2`,
 *   τ_shift_low = 0.20, τ_shift_mid = 0.50):
 *     Each: `name`, `σ_shift ∈ [0, 1]`,
 *     `bits ∈ {BIT1, BIT2, BIT4}`,
 *     cascade:
 *       σ_shift ≤ 0.20 → BIT1
 *       σ_shift ≤ 0.50 → BIT2
 *       else           → BIT4.
 *     Contract: every branch fires exactly once.
 *
 *   σ_moe (surface hygiene):
 *       σ_moe = 1 −
 *         (route_rows_ok + route_both_ok +
 *          sig_rows_ok + sig_both_ok +
 *          prefetch_rows_ok + prefetch_all_ok +
 *          mobie_rows_ok + mobie_all_ok) /
 *         (4 + 1 + 3 + 1 + 3 + 1 + 3 + 1)
 *   v0 requires `σ_moe == 0.0`.
 *
 *   Contracts (v0):
 *     1. 4 routing rows; decision matches σ vs
 *        τ_route; both branches fire.
 *     2. 3 signature rows canonical; familiar matches
 *        entropy vs τ_sig; both branches fire.
 *     3. 3 prefetch rows canonical; cascade assigns
 *        each strategy exactly once.
 *     4. 3 MoBiE rows canonical; bits cascade assigns
 *        each width exactly once.
 *     5. σ_moe ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v280.1 (named, not implemented): live MoE router
 *     wired into v262 with σ-per-token activation
 *     budget, measured routing entropy per task on a
 *     real 8x-of-64 model, and adaptive
 *     MoBiE-quantisation driven by measured σ_shift
 *     per expert.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V280_MOE_H
#define COS_V280_MOE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V280_N_ROUTE     4
#define COS_V280_N_SIG       3
#define COS_V280_N_PREFETCH  3
#define COS_V280_N_MOBIE     3

typedef enum {
    COS_V280_ROUTE_TOP_K     = 1,
    COS_V280_ROUTE_DIVERSIFY = 2
} cos_v280_route_t;

typedef enum {
    COS_V280_FAM_KNOWN = 1,
    COS_V280_FAM_NOVEL = 2
} cos_v280_fam_t;

typedef enum {
    COS_V280_STRAT_AGGRESSIVE   = 1,
    COS_V280_STRAT_BALANCED     = 2,
    COS_V280_STRAT_CONSERVATIVE = 3
} cos_v280_strat_t;

typedef enum {
    COS_V280_BITS_BIT1 = 1,
    COS_V280_BITS_BIT2 = 2,
    COS_V280_BITS_BIT4 = 4
} cos_v280_bits_t;

typedef struct {
    int               token_id;
    float             sigma_routing;
    cos_v280_route_t  decision;
} cos_v280_route_row_t;

typedef struct {
    char            task[10];
    float           routing_entropy;
    cos_v280_fam_t  familiar;
} cos_v280_sig_t;

typedef struct {
    char              label[6];
    float             sigma_prefetch;
    cos_v280_strat_t  strategy;
} cos_v280_prefetch_t;

typedef struct {
    char             name[10];
    float            sigma_shift;
    cos_v280_bits_t  bits;
} cos_v280_mobie_t;

typedef struct {
    cos_v280_route_row_t  route    [COS_V280_N_ROUTE];
    cos_v280_sig_t        sig      [COS_V280_N_SIG];
    cos_v280_prefetch_t   prefetch [COS_V280_N_PREFETCH];
    cos_v280_mobie_t      mobie    [COS_V280_N_MOBIE];

    float tau_route;         /* 0.35 */
    float tau_sig;           /* 0.40 */
    float tau_prefetch_low;  /* 0.20 */
    float tau_prefetch_mid;  /* 0.50 */
    float tau_shift_low;     /* 0.20 */
    float tau_shift_mid;     /* 0.50 */

    int   n_route_rows_ok;
    int   n_route_topk;
    int   n_route_diversify;

    int   n_sig_rows_ok;
    int   n_sig_known;
    int   n_sig_novel;

    int   n_prefetch_rows_ok;
    int   n_prefetch_agg;
    int   n_prefetch_bal;
    int   n_prefetch_cons;

    int   n_mobie_rows_ok;
    int   n_mobie_bit1;
    int   n_mobie_bit2;
    int   n_mobie_bit4;

    float sigma_moe;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v280_state_t;

void   cos_v280_init(cos_v280_state_t *s, uint64_t seed);
void   cos_v280_run (cos_v280_state_t *s);

size_t cos_v280_to_json(const cos_v280_state_t *s,
                         char *buf, size_t cap);

int    cos_v280_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V280_MOE_H */
