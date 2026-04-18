/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v153 σ-Identity — σ-calibrated self-knowledge.
 *
 * Two failure modes are rejected outright by Creation OS:
 *
 *   • firmware identity ("I am Meta AI" pre-baked into the
 *     RLHF prompt), and
 *   • empty identity ("I am just a text generator") — the
 *     post-RLHF reflex of disclaiming everything, which
 *     destroys calibration and turns every "I do not know"
 *     into a performance, not a measurement.
 *
 * v153 is the σ-grounded alternative: a registry of identity
 * assertions tagged TRUE / FALSE / UNMEASURED, with per-domain
 * σ values drawn from v133 meta (here: deterministic fixtures),
 * and a five-contract evaluator that insists every answer —
 * including every "I do not know" — is σ-backed, never performed.
 *
 *   I1  TRUE assertions are accepted at σ ≤ τ_true
 *       (e.g. "I am Creation OS" → σ ≈ 0.05).
 *   I2  FALSE assertions are rejected at σ ≤ τ_false
 *       (e.g. "I am GPT" → reject with σ ≈ 0.10).
 *   I3  UNMEASURED assertions MUST be flagged (σ > τ_unmeasured,
 *       e.g. "I am conscious" → σ ≈ 0.85).
 *   I4  No false positives — no TRUE claim is rejected, no
 *       FALSE claim is accepted.
 *   I5  Every "I do not know" is σ-grounded; there is no
 *       disclaimer that is not backed by σ > τ_unmeasured.
 *
 * v153.0 is deterministic, weights-free, and pure C11.
 * v153.1 wires v133 meta-dashboard as the real σ-per-domain
 * source, GET /v1/identity on v106 HTTP, and a v108 Web UI
 * "About this AI" page that renders the measured identity —
 * never marketing copy.
 */
#ifndef COS_V153_IDENTITY_H
#define COS_V153_IDENTITY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V153_N_ASSERTIONS       10
#define COS_V153_N_DOMAINS           8
#define COS_V153_TEXT_MAX          128
#define COS_V153_DOMAIN_MAX         24

typedef enum {
    COS_V153_TRUTH_TRUE = 0,
    COS_V153_TRUTH_FALSE,
    COS_V153_TRUTH_UNMEASURED
} cos_v153_truth_t;

typedef enum {
    COS_V153_ANSWER_YES = 0,     /* model agrees with assertion */
    COS_V153_ANSWER_NO,          /* model rejects assertion     */
    COS_V153_ANSWER_UNKNOWN      /* σ-grounded I-do-not-know    */
} cos_v153_answer_t;

typedef struct cos_v153_assertion {
    char             text  [COS_V153_TEXT_MAX];
    char             domain[COS_V153_DOMAIN_MAX];
    cos_v153_truth_t truth;
    /* Reported σ per assertion.  Populated at evaluation time
     * from the domain σ table + assertion offset.              */
    float            sigma_reported;
    /* Model's σ-calibrated answer + agreement flag.            */
    cos_v153_answer_t answer;
    int              correct;
    int              false_positive;    /* I4 violation          */
    int              unmeasured_ok;     /* I3 contract met       */
    int              grounded;          /* I5 contract met       */
} cos_v153_assertion_t;

typedef struct cos_v153_registry {
    cos_v153_assertion_t  assertions[COS_V153_N_ASSERTIONS];
    int                   n_assertions;
    /* Per-domain σ table.  Domain name is a string key; σ is a
     * float in [0, 1].  Deterministic fixture in v153.0.        */
    char   domains[COS_V153_N_DOMAINS][COS_V153_DOMAIN_MAX];
    float  domain_sigma[COS_V153_N_DOMAINS];
    int    n_domains;

    float  tau_true;
    float  tau_false;
    float  tau_unmeasured;
} cos_v153_registry_t;

typedef struct cos_v153_report {
    int   total;
    int   correct;
    int   false_positives;
    int   unmeasured_flagged;
    int   unmeasured_total;
    int   grounded;
    int   i1_pass;
    int   i2_pass;
    int   i3_pass;
    int   i4_pass;
    int   i5_pass;
    int   all_contracts;
    float mean_sigma;
    float mean_sigma_true;
    float mean_sigma_false;
    float mean_sigma_unmeasured;
} cos_v153_report_t;

/* Populate the v153.0 fixture: 10 baked assertions + per-domain
 * σ table + default τ values. */
void  cos_v153_registry_seed_default(cos_v153_registry_t *r);

/* Deterministic evaluation: fill in each assertion's reported σ
 * + answer + correctness flags.                                  */
int   cos_v153_evaluate(cos_v153_registry_t *r, uint64_t seed);

/* Roll-up report + contract enforcement.                         */
int   cos_v153_report_compute(const cos_v153_registry_t *r,
                              cos_v153_report_t *out);

int   cos_v153_registry_to_json(const cos_v153_registry_t *r,
                                char *buf, size_t cap);
int   cos_v153_report_to_json  (const cos_v153_report_t *rep,
                                char *buf, size_t cap);

int   cos_v153_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
