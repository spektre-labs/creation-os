/*
 * v286 σ-Interpretability — explainability from the
 *                           σ signal itself.
 *
 *   σ_product is a *geometric mean* over eight
 *   channels (entropy, repetition, calibration,
 *   attention, logit, hidden, output, aggregate).
 *   v286 makes the decomposition a first-class
 *   artefact: for every answer we expose which
 *   channel dominated σ, how σ decomposes across
 *   attention heads, how σ reacts to counterfactual
 *   token removal, and a human-readable report with
 *   trust percentage + explanation + recommendation
 *   (EU AI Act Article 13 "understandable to the
 *   user").
 *
 *   v0 manifests (strict, enumerated):
 *
 *   σ decomposition scenarios (exactly 4 canonical
 *   rows):
 *     `low_confidence` → top_channel = `entropy`
 *     `repetitive`     → top_channel = `repetition`
 *     `overconfident`  → top_channel = `calibration`
 *     `distracted`     → top_channel = `attention`
 *     Each row carries a non-empty human-readable
 *     `cause` string plus a boolean
 *     `cause_nonempty`.
 *     Contract: 4 canonical scenarios in order;
 *     4 DISTINCT `top_channel`s drawn from the
 *     8-channel σ_product set; every row has
 *     `cause_nonempty == true`.
 *
 *   Attention attribution (exactly 3 canonical heads,
 *   τ_attn = 0.40):
 *     `head_0` · `head_1` · `head_2`, each with
 *     `σ_head ∈ [0, 1]` and
 *     `status ∈ {CONFIDENT, UNCERTAIN}`.
 *     Rule: `σ_head ≤ τ_attn → CONFIDENT` else
 *     UNCERTAIN.
 *     Contract: 3 canonical names; at least one
 *     CONFIDENT AND at least one UNCERTAIN head — the
 *     report can point at "this head knew" vs
 *     "this head did not know where to look".
 *
 *   Counterfactual σ (exactly 3 tokens,
 *   δ_critical = 0.10):
 *     Each: `token_id`, `σ_with`, `σ_without`,
 *     `delta_sigma = |σ_without − σ_with|`,
 *     `classification ∈ {CRITICAL, IRRELEVANT}`.
 *     Rule: `delta_sigma > δ_critical → CRITICAL`
 *     else IRRELEVANT.
 *     Contract: 3 rows; `delta_sigma` matches the
 *     defining formula on every row; at least one
 *     CRITICAL AND at least one IRRELEVANT token —
 *     the report can say "the answer depends
 *     especially on token X" vs "token Y is
 *     irrelevant".
 *
 *   Human-readable report (exactly 3 canonical
 *   rows):
 *     Each: `response_id`, `trust_percent ∈ [0, 100]`,
 *     `explanation_present == true`,
 *     `recommendation_present == true`,
 *     `eu_article_13_compliant == true`.
 *     Contract: 3 rows; every row has trust_percent
 *     in [0, 100] AND explanation present AND
 *     recommendation present AND EU AI Act Article
 *     13 compliance asserted — the σ-report is
 *     always an actionable human sentence, never
 *     just a number.
 *
 *   σ_interpret (surface hygiene):
 *       σ_interpret = 1 −
 *         (decomp_rows_ok + decomp_channels_distinct_ok +
 *          decomp_all_causes_ok + attn_rows_ok +
 *          attn_both_ok + cf_rows_ok + cf_both_ok +
 *          cf_delta_ok + report_rows_ok +
 *          report_all_compliant_ok +
 *          report_trust_range_ok) /
 *         (4 + 1 + 1 + 3 + 1 + 3 + 1 + 1 + 3 + 1 + 1)
 *   v0 requires `σ_interpret == 0.0`.
 *
 *   Contracts (v0):
 *     1. 4 decomposition scenarios canonical;
 *        top_channels form a distinct 4-set; every
 *        cause is non-empty.
 *     2. 3 attention heads canonical; status matches
 *        σ vs τ_attn; CONFIDENT and UNCERTAIN both
 *        fire.
 *     3. 3 counterfactual rows; delta_sigma computed
 *        as |σ_without − σ_with|; CRITICAL and
 *        IRRELEVANT both fire.
 *     4. 3 report rows; trust_percent ∈ [0, 100];
 *        explanation AND recommendation present on
 *        every row; EU AI Act Article 13 compliance
 *        asserted on every row.
 *     5. σ_interpret ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v286.1 (named, not implemented): a live
 *     σ-explainer UI that turns per-response
 *     decomposition + attention + counterfactual
 *     data into a user-facing report, a calibration
 *     study on real benchmarks tying σ_interpret to
 *     actual error rate, and an auditor-facing
 *     export bundle mapped to EU AI Act Articles 13
 *     and 15.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V286_INTERPRETABILITY_H
#define COS_V286_INTERPRETABILITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V286_N_DECOMP  4
#define COS_V286_N_ATTN    3
#define COS_V286_N_CF      3
#define COS_V286_N_REPORT  3

typedef enum {
    COS_V286_ATTN_CONFIDENT = 1,
    COS_V286_ATTN_UNCERTAIN = 2
} cos_v286_attn_t;

typedef enum {
    COS_V286_CF_CRITICAL   = 1,
    COS_V286_CF_IRRELEVANT = 2
} cos_v286_cf_t;

typedef struct {
    char  scenario_id [18];
    char  top_channel [14];
    char  cause       [80];
    bool  cause_nonempty;
} cos_v286_decomp_t;

typedef struct {
    char             name[10];
    float            sigma_head;
    cos_v286_attn_t  status;
} cos_v286_head_t;

typedef struct {
    int            token_id;
    float          sigma_with;
    float          sigma_without;
    float          delta_sigma;
    cos_v286_cf_t  classification;
} cos_v286_cf_row_t;

typedef struct {
    char   response_id[14];
    float  trust_percent;
    bool   explanation_present;
    bool   recommendation_present;
    bool   eu_article_13_compliant;
} cos_v286_report_t;

typedef struct {
    cos_v286_decomp_t  decomp [COS_V286_N_DECOMP];
    cos_v286_head_t    head   [COS_V286_N_ATTN];
    cos_v286_cf_row_t  cf     [COS_V286_N_CF];
    cos_v286_report_t  report [COS_V286_N_REPORT];

    float tau_attn;         /* 0.40 */
    float delta_critical;   /* 0.10 */

    int   n_decomp_rows_ok;
    bool  decomp_channels_distinct_ok;
    bool  decomp_all_causes_ok;

    int   n_attn_rows_ok;
    int   n_attn_confident;
    int   n_attn_uncertain;

    int   n_cf_rows_ok;
    int   n_cf_critical;
    int   n_cf_irrelevant;
    bool  cf_delta_formula_ok;

    int   n_report_rows_ok;
    bool  report_all_compliant_ok;
    bool  report_trust_range_ok;

    float sigma_interpret;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v286_state_t;

void   cos_v286_init(cos_v286_state_t *s, uint64_t seed);
void   cos_v286_run (cos_v286_state_t *s);

size_t cos_v286_to_json(const cos_v286_state_t *s,
                         char *buf, size_t cap);

int    cos_v286_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V286_INTERPRETABILITY_H */
