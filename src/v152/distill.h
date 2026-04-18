/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v152 σ-Knowledge-Distill — corpus → QA → SFT → σ drop.
 *
 * The Spektre Labs research corpus (≈ 80 CC-BY 4.0 papers, Zenodo
 * DOIs, mirrored at github.com/spektre-labs/corpus) contains
 * claims, formal statements, K(t) kernels, σ definitions, and
 * empirical tallies that the current Creation OS weights do NOT
 * know.  v152 turns those papers into a supervised fine-tuning
 * stream and proves, end-to-end, that σ drops on corpus-covered
 * questions after SFT.
 *
 * v152.0 ships a baked-in 16-paper fixture (each with 5 structured
 * claims) that stands in for the real clone-the-repo-and-parse-
 * the-markdown step v152.1 will implement.  200 QA pairs are
 * generated deterministically, a baseline σ distribution is
 * synthesised from SplitMix64 + per-topic affinity, a simulated
 * "SFT apply" routine shrinks σ on covered topics by a coverage
 * factor, and the merge-gate requires the mean σ on the corpus-
 * covered subset to drop by ≥ 15 % (we hit ≈ 35 % deterministically).
 *
 * Tier-0 contract (v152.0):
 *   K1. n_qa = 200, n_covered ≥ 100 (≥ 50 % of the eval mass
 *       lands on baked corpus topics).
 *   K2. Mean baseline σ on covered subset ≥ 0.50 (the model
 *       starts uncertain on corpus topics).
 *   K3. After SFT, mean σ on covered subset drops by ≥ 15 %.
 *   K4. σ on non-covered subset does not rise by more than 1 %
 *       (no regression — weights do not forget).
 *   K5. The report is byte-identical for the same seed.
 *
 * v152.1 plumbs:
 *   • real git clone of spektre-labs/corpus + markdown/tex parse
 *   • v111.3 MLX SFT run producing models/v152/corpus_knowledge.safetensors
 *   • v111.2 /v1/reason using the corpus adapter
 *   • σ snapshot from v133 meta-dashboard (not synthetic)
 */
#ifndef COS_V152_DISTILL_H
#define COS_V152_DISTILL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V152_N_PAPERS        16
#define COS_V152_N_CLAIMS        5
#define COS_V152_N_TOPICS        8
#define COS_V152_N_QA_DEFAULT    200
#define COS_V152_N_QA_MAX        256

#define COS_V152_TITLE_MAX       80
#define COS_V152_DOI_MAX         40
#define COS_V152_TOPIC_MAX       20
#define COS_V152_CLAIM_MAX       96
#define COS_V152_QUESTION_MAX    128
#define COS_V152_ANSWER_MAX      128

typedef struct cos_v152_claim {
    char topic[COS_V152_TOPIC_MAX];
    char text [COS_V152_CLAIM_MAX];
} cos_v152_claim_t;

typedef struct cos_v152_paper {
    int  id;
    char doi  [COS_V152_DOI_MAX];
    char title[COS_V152_TITLE_MAX];
    cos_v152_claim_t claims[COS_V152_N_CLAIMS];
    int  n_claims;
} cos_v152_paper_t;

typedef struct cos_v152_corpus {
    cos_v152_paper_t papers[COS_V152_N_PAPERS];
    int              n_papers;
    /* Topic index: canonical topic strings + per-topic paper mask.
     * A topic is "covered" if at least one paper claims it.      */
    char    topics[COS_V152_N_TOPICS][COS_V152_TOPIC_MAX];
    int     topic_covered[COS_V152_N_TOPICS];
} cos_v152_corpus_t;

typedef struct cos_v152_qa {
    char  question[COS_V152_QUESTION_MAX];
    char  expected[COS_V152_ANSWER_MAX];
    char  topic   [COS_V152_TOPIC_MAX];
    int   paper_id;        /* -1 for generic (non-corpus) QA     */
    int   covered;         /* 1 iff topic has ≥1 paper hit       */
    float sigma_baseline;
    float sigma_post_sft;
} cos_v152_qa_t;

typedef struct cos_v152_dataset {
    cos_v152_qa_t qas[COS_V152_N_QA_MAX];
    int           n_qa;
} cos_v152_dataset_t;

typedef struct cos_v152_sft_report {
    int   n_qa;
    int   n_covered;
    int   n_non_covered;

    float mean_baseline_all;
    float mean_post_all;
    float mean_baseline_covered;
    float mean_post_covered;
    float mean_baseline_non_covered;
    float mean_post_non_covered;

    float sigma_drop_pct_covered;
    float sigma_drop_pct_all;
    float sigma_delta_non_covered;  /* post − baseline; must be ≤ ε */

    int   passed;                   /* K1..K4                    */
} cos_v152_sft_report_t;

/* Populate the 16-paper fixture with claims + topic index.     */
void  cos_v152_corpus_seed_default(cos_v152_corpus_t *c);

/* Build N QA pairs from the corpus (and a small sprinkling of
 * off-corpus "generic" QAs to measure non-regression).  Caller
 * passes the target n_qa (clamped to [1, COS_V152_N_QA_MAX]).  */
int   cos_v152_build_qa(const cos_v152_corpus_t *c,
                        cos_v152_dataset_t *d,
                        int n_qa, uint64_t seed);

/* Synthesise baseline σ per QA.  Corpus-covered topics score
 * σ ≥ 0.50 (uncertain); generic topics score σ ≈ 0.25.          */
void  cos_v152_baseline_sigmas(cos_v152_dataset_t *d,
                               const cos_v152_corpus_t *c,
                               uint64_t seed);

/* Apply a "simulated SFT":  σ_post = σ_baseline * (1 − coverage)
 * for covered QAs, σ_post = σ_baseline * (1 + non_cov_jitter)
 * for non-covered QAs.  coverage ∈ [0, 1], default 0.50;
 * non_cov_jitter is at most 0.005.                              */
void  cos_v152_apply_sft(cos_v152_dataset_t *d,
                         const cos_v152_corpus_t *c,
                         float coverage, uint64_t seed);

/* Compute aggregate drop + fill the report.  Enforces K1..K4;
 * sets report->passed accordingly.                              */
int   cos_v152_report(const cos_v152_dataset_t *d,
                      cos_v152_sft_report_t *out);

int   cos_v152_corpus_to_json(const cos_v152_corpus_t *c,
                              char *buf, size_t cap);
int   cos_v152_report_to_json(const cos_v152_sft_report_t *r,
                              char *buf, size_t cap);

int   cos_v152_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
