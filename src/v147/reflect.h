/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v147 σ-Reflect — deep self-reflection kernel.
 *
 * v133 meta measures *outcome* (σ at answer-time).  v147 goes
 * one level deeper: it records a *thought trace* from the
 * v111.2 reasoning loop, tags each intermediate step with its
 * own σ-profile, and asks two structural questions:
 *
 *   R1  Consistency: the same query answered *without* showing
 *       the chain of thought yields a "pure" answer.  Does the
 *       pure answer agree with the chain answer?  If they
 *       disagree, at least one intermediate step introduced
 *       an error — the chain is *doing work* but the wrong way.
 *
 *   R2  Weakest link: the thought with the highest σ_product
 *       is the most uncertain reasoning step, by construction
 *       the most likely place the error entered.  v147 returns
 *       that index + σ for targeted regeneration.
 *
 *   R3  RAIN rewind depth (ICLR 2024 — Rewindable Auto-regressive
 *       INference): if self-eval rejects the current answer, how
 *       far to rewind?  σ of the weakest step controls:
 *          σ ≤ 0.30                → depth 1 (local fix)
 *          0.30 <  σ ≤ 0.60        → depth ⌈n/2⌉ (mid-chain)
 *          σ >  0.60               → depth n     (restart chain)
 *
 * v147.0 is deterministic, string-free at the semantic level:
 * every "answer" is a normalised hash, every σ is explicit.  No
 * model calls, no embeddings — the contract is shape-level.  A
 * v147.1 follow-up wires v111.2 to feed real thought traces and
 * v125 DPO to use the weakest-step index as a training signal.
 */
#ifndef COS_V147_REFLECT_H
#define COS_V147_REFLECT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V147_MAX_THOUGHTS   16
#define COS_V147_TEXT_CAP       96

typedef struct cos_v147_thought {
    int    index;
    char   text[COS_V147_TEXT_CAP];
    float  sigma_product;        /* σ of this reasoning step          */
} cos_v147_thought_t;

typedef struct cos_v147_trace {
    int                   n_thoughts;
    cos_v147_thought_t    thoughts[COS_V147_MAX_THOUGHTS];
    char                  final_answer[COS_V147_TEXT_CAP];
    float                 final_sigma;     /* σ on the joint answer   */
} cos_v147_trace_t;

typedef struct cos_v147_reflection {
    int    weakest_step;             /* argmax σ_product; -1 if empty */
    float  weakest_sigma;
    int    rewind_depth;             /* 1, ⌈n/2⌉ or n (contract R3)   */
    int    consistency_ok;           /* 1 iff pure_answer == final    */
    int    divergence_detected;      /* !consistency_ok               */
    float  mean_sigma;               /* mean of thought σ values      */
    float  max_sigma;
    float  min_sigma;
    int    rain_should_rewind;       /* 1 iff σ_weakest > τ_rewind    */
} cos_v147_reflection_t;

/* Initialise an empty trace with a final answer + joint σ. */
void cos_v147_trace_init(cos_v147_trace_t *t,
                         const char *final_answer,
                         float final_sigma);

/* Append one thought step.  Returns 0 on success, <0 on full. */
int  cos_v147_trace_add(cos_v147_trace_t *t,
                        const char *text,
                        float sigma_product);

/* Compute the RAIN rewind depth from the weakest σ and chain
 * length.  Deterministic, side-effect free — exposed so the
 * sovereign loop (v148) can call it directly. */
int  cos_v147_rewind_depth(float weakest_sigma, int n_steps);

/* Run the reflection step:
 *   1) identify the weakest thought (R2),
 *   2) compute rewind depth from σ_weakest (R3),
 *   3) compare pure_answer vs trace->final_answer (R1),
 *   4) fill the reflection report.
 * Returns 0 on success, <0 on arg error.  pure_sigma is captured
 * for symmetry (the sovereign loop pairs it with final_sigma to
 * decide whether the disagreement is within σ-noise). */
int  cos_v147_reflect(const cos_v147_trace_t *trace,
                      const char *pure_answer,
                      float pure_sigma,
                      cos_v147_reflection_t *out);

int  cos_v147_trace_to_json(const cos_v147_trace_t *t,
                            char *buf, size_t cap);
int  cos_v147_reflection_to_json(const cos_v147_reflection_t *r,
                                 const cos_v147_trace_t *t,
                                 char *buf, size_t cap);

int  cos_v147_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
