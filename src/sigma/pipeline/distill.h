/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Distill — continuous σ-guided distillation from escalation pairs.
 *
 * Every time the σ-pipeline escalates an input from the local student
 * (BitNet 2B) to a teacher (Claude / GPT / DeepSeek / any API), the
 * resulting (input, student_answer, teacher_answer, σ) quadruple is
 * free supervised training data.  σ-distill turns that stream into
 * a continuously-improving student.
 *
 * Pair value model
 * ----------------
 * A distillation pair is valuable iff the student was uncertain and
 * the teacher was confident.  We score:
 *
 *     σ_distill_value = student_σ - teacher_σ
 *
 * (clipped to [-1, 1]), so that:
 *
 *   - student_σ high + teacher_σ low  → value close to +1  (gold)
 *   - both low                        → value ~  0  (not needed)
 *   - both high                       → value ~  0  (teacher was
 *                                                  not helpful)
 *   - student_σ low + teacher_σ high  → value negative (skip!)
 *
 * σ-distill keeps a ring buffer of the most recent pairs, filters
 * those with teacher_sigma < tau_teacher (confident teacher only,
 * default 0.20), ranks the survivors by σ_distill_value, and hands
 * the top-K back for TTT / P6 fast-weight update on the student.
 * P16 continual-learning rules outside this module are responsible
 * for protecting old skills; σ-distill only provides the curriculum.
 *
 * Contracts (v0)
 * --------------
 *   1. Bounded memory: at most DISTILL_RING_CAP pairs live at once;
 *      FIFO eviction after that.
 *   2. Ownership: the caller retains ownership of every string in
 *      `cos_sigma_distill_pair_t`; σ-distill copies them on `append`
 *      and frees them on `free` / ring eviction.  No pointer stays
 *      aliased outside this module.
 *   3. Determinism: given the same sequence of appends,
 *      `cos_sigma_distill_select` returns a byte-identical list of
 *      indices on any tier-1 host (stable sort key = value, tie =
 *      insertion order).
 *   4. σ bounds: every reported σ is clip01'd; selection and value
 *      math never see NaN even if the caller supplies it.
 *   5. Escalation accounting: `cos_sigma_distill_stats` reports
 *      total_seen, filtered_in (teacher_σ <= τ_teacher), top_k, and
 *      mean σ_distill_value of the filtered-in subset — everything
 *      a benchmark needs to show "escalation rate is dropping".
 *
 * This module does NOT itself update BitNet weights — that is the
 * TTT kernel's job (P6).  σ-distill is the curriculum generator
 * and the σ-receipt that proves the curriculum was σ-selected,
 * not mined at random.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_DISTILL_H
#define COS_SIGMA_DISTILL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COS_DISTILL_RING_CAP
#define COS_DISTILL_RING_CAP 1024  /* ≥ 1000 as per spec */
#endif

#ifndef COS_DISTILL_TAU_TEACHER_DEFAULT
#define COS_DISTILL_TAU_TEACHER_DEFAULT 0.20f
#endif

enum cos_distill_status {
    COS_DISTILL_OK         =  0,
    COS_DISTILL_ERR_ARG    = -1,
    COS_DISTILL_ERR_OOM    = -2,
    COS_DISTILL_ERR_EMPTY  = -3,
};

/* Caller-supplied input to cos_sigma_distill_append.  Strings are
 * copied and may be freed immediately after the call returns. */
typedef struct {
    const char *input;           /* prompt that was routed          */
    const char *student_answer;  /* BitNet's attempt                */
    float       student_sigma;   /* σ the pipeline saw on student   */
    const char *teacher_answer;  /* escalated teacher's answer      */
    float       teacher_sigma;   /* σ on teacher (if reported)      */
    uint64_t    timestamp_ns;    /* caller-monotonic ns; for replay */
} cos_sigma_distill_pair_t;

/* Per-pair record stored inside the ring; owned by σ-distill. */
typedef struct {
    char    *input;
    char    *student_answer;
    char    *teacher_answer;
    float    student_sigma;      /* clip01'd                         */
    float    teacher_sigma;      /* clip01'd                         */
    float    value;              /* σ_distill_value ∈ [-1, 1]        */
    uint64_t timestamp_ns;
    uint64_t insertion_order;    /* stable-sort tie-breaker          */
} cos_sigma_distill_record_t;

/* Stats + summary; every accept / reject accumulates here. */
typedef struct {
    uint64_t total_seen;         /* appends attempted                */
    uint64_t filtered_in;        /* teacher_σ ≤ τ_teacher            */
    uint64_t evicted;            /* ring FIFO evictions              */
    float    mean_value_filt;    /* mean σ_distill_value in `filt`   */
    float    mean_student_filt;  /* mean student σ in `filt`         */
    float    mean_teacher_filt;  /* mean teacher σ in `filt`         */
} cos_sigma_distill_stats_t;

typedef struct {
    cos_sigma_distill_record_t ring[COS_DISTILL_RING_CAP];
    int      size;               /* live entries in ring             */
    int      head;               /* next write slot (FIFO)           */
    float    tau_teacher;        /* filter threshold                 */
    uint64_t insertion_counter;  /* monotonic for tie-breaking       */
    cos_sigma_distill_stats_t stats;
} cos_sigma_distill_state_t;

/* -------- Lifecycle ------------------------------------------------ */

/* Zero-initialise `st` and set τ_teacher.  Pass 0.0 to accept the
 * compile-time default COS_DISTILL_TAU_TEACHER_DEFAULT. */
int cos_sigma_distill_init(cos_sigma_distill_state_t *st,
                           float tau_teacher);

/* Free every string owned by the ring.  `st` is left in a clean
 * zero state (it can be reused with a fresh init). */
void cos_sigma_distill_free(cos_sigma_distill_state_t *st);

/* -------- Ingest -------------------------------------------------- */

/* Append a pair, applying the filter and FIFO-evicting the oldest
 * record once the ring is full.  Returns COS_DISTILL_OK if the pair
 * was stored (either new slot or after eviction); returns negative
 * on allocation failure. */
int cos_sigma_distill_append(cos_sigma_distill_state_t *st,
                             const cos_sigma_distill_pair_t *pair);

/* -------- Select -------------------------------------------------- */

/* Return the indices of the top-K records sorted by σ_distill_value
 * (descending), restricted to records with teacher_σ ≤ τ_teacher
 * and student_σ ≥ student_sigma_floor.  Caller provides `out_idx`
 * of capacity `k`; the number of entries actually written is stored
 * in `*out_n` (0 ≤ *out_n ≤ k).  Returns COS_DISTILL_OK on success
 * (even if *out_n == 0). */
int cos_sigma_distill_select(const cos_sigma_distill_state_t *st,
                             int k,
                             float student_sigma_floor,
                             int *out_idx,
                             int *out_n);

/* -------- Introspection ------------------------------------------- */

float cos_sigma_distill_value(float student_sigma, float teacher_sigma);

/* Snapshot the stats; never fails. */
void cos_sigma_distill_stats(const cos_sigma_distill_state_t *st,
                             cos_sigma_distill_stats_t *out);

/* Read-only borrow of a record (pointers alias the ring; do not
 * free). Returns NULL if idx is out of range. */
const cos_sigma_distill_record_t *
cos_sigma_distill_at(const cos_sigma_distill_state_t *st, int idx);

/* -------- Self-test ----------------------------------------------- */

int cos_sigma_distill_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_DISTILL_H */
