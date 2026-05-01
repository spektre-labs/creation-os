/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Distill implementation.  See distill.h for contracts.
 *
 * Data path
 * ---------
 *   append(pair)
 *     1. clip01(student_σ, teacher_σ)
 *     2. value = clip(student_σ − teacher_σ, [-1, 1])
 *     3. copy strings (malloc + memcpy; callers keep ownership of
 *        their originals), free the evicted slot's strings, write
 *        new record at st->head, advance head mod CAP
 *     4. if teacher_σ ≤ τ_teacher: bump filtered_in + update
 *        running means (Welford on the filtered subset)
 *
 *   select(k, floor)
 *     1. scan ring, collect candidate indices with teacher_σ ≤ τ
 *        AND student_σ ≥ floor
 *     2. insertion-sort by (-value, insertion_order) — N is ≤ 1024
 *        so O(N²) is fine and preserves determinism
 *     3. write first min(k, n_candidates) indices into out_idx
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "distill.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 * Small helpers                                                      *
 * ------------------------------------------------------------------ */

static float distill_clip01(float x) {
    if (!(x == x))   return 0.0f;   /* NaN */
    if (x < 0.0f)    return 0.0f;
    if (x > 1.0f)    return 1.0f;
    return x;
}

static float distill_clipunit(float x) {
    if (!(x == x))   return 0.0f;
    if (x < -1.0f)   return -1.0f;
    if (x >  1.0f)   return  1.0f;
    return x;
}

static char *distill_strdup(const char *s) {
    if (!s) {
        char *e = (char *)malloc(1);
        if (!e) return NULL;
        e[0] = '\0';
        return e;
    }
    size_t n = strlen(s);
    char  *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

static void distill_record_free(cos_sigma_distill_record_t *r) {
    if (!r) return;
    free(r->input);
    free(r->student_answer);
    free(r->teacher_answer);
    memset(r, 0, sizeof *r);
}

/* ------------------------------------------------------------------ *
 * Public                                                             *
 * ------------------------------------------------------------------ */

float cos_sigma_distill_value(float student_sigma, float teacher_sigma) {
    float s = distill_clip01(student_sigma);
    float t = distill_clip01(teacher_sigma);
    return distill_clipunit(s - t);
}

int cos_sigma_distill_init(cos_sigma_distill_state_t *st,
                           float tau_teacher) {
    if (!st) return COS_DISTILL_ERR_ARG;
    memset(st, 0, sizeof *st);
    if (tau_teacher <= 0.0f) tau_teacher = COS_DISTILL_TAU_TEACHER_DEFAULT;
    st->tau_teacher = distill_clip01(tau_teacher);
    return COS_DISTILL_OK;
}

void cos_sigma_distill_free(cos_sigma_distill_state_t *st) {
    if (!st) return;
    for (int i = 0; i < COS_DISTILL_RING_CAP; i++)
        distill_record_free(&st->ring[i]);
    float tau = st->tau_teacher;
    memset(st, 0, sizeof *st);
    st->tau_teacher = tau;
}

int cos_sigma_distill_append(cos_sigma_distill_state_t *st,
                             const cos_sigma_distill_pair_t *pair) {
    if (!st || !pair) return COS_DISTILL_ERR_ARG;

    st->stats.total_seen++;

    cos_sigma_distill_record_t *slot = &st->ring[st->head];

    /* Track eviction of the slot we are about to overwrite. */
    int evicted = (slot->input != NULL);
    distill_record_free(slot);

    slot->input          = distill_strdup(pair->input);
    slot->student_answer = distill_strdup(pair->student_answer);
    slot->teacher_answer = distill_strdup(pair->teacher_answer);
    if (!slot->input || !slot->student_answer || !slot->teacher_answer) {
        distill_record_free(slot);
        return COS_DISTILL_ERR_OOM;
    }
    slot->student_sigma   = distill_clip01(pair->student_sigma);
    slot->teacher_sigma   = distill_clip01(pair->teacher_sigma);
    slot->value           = cos_sigma_distill_value(slot->student_sigma,
                                                   slot->teacher_sigma);
    slot->timestamp_ns    = pair->timestamp_ns;
    slot->insertion_order = ++st->insertion_counter;

    if (evicted)               st->stats.evicted++;
    if (st->size < COS_DISTILL_RING_CAP) st->size++;
    st->head = (st->head + 1) % COS_DISTILL_RING_CAP;

    if (slot->teacher_sigma <= st->tau_teacher) {
        /* Running mean update (Welford's simplification for count). */
        uint64_t n = ++st->stats.filtered_in;
        float    inv = 1.0f / (float)n;
        st->stats.mean_value_filt   += (slot->value          - st->stats.mean_value_filt  ) * inv;
        st->stats.mean_student_filt += (slot->student_sigma  - st->stats.mean_student_filt) * inv;
        st->stats.mean_teacher_filt += (slot->teacher_sigma  - st->stats.mean_teacher_filt) * inv;
    }
    return COS_DISTILL_OK;
}

int cos_sigma_distill_select(const cos_sigma_distill_state_t *st,
                             int k,
                             float student_sigma_floor,
                             int *out_idx,
                             int *out_n) {
    if (!st || !out_idx || !out_n || k <= 0) return COS_DISTILL_ERR_ARG;
    *out_n = 0;

    float floor = distill_clip01(student_sigma_floor);
    int   cand[COS_DISTILL_RING_CAP];
    int   n_cand = 0;

    for (int i = 0; i < COS_DISTILL_RING_CAP; i++) {
        const cos_sigma_distill_record_t *r = &st->ring[i];
        if (!r->input) continue;
        if (r->teacher_sigma > st->tau_teacher) continue;
        if (r->student_sigma < floor)           continue;
        cand[n_cand++] = i;
    }

    /* Insertion sort by (-value, insertion_order ASC).  Deterministic
     * and O(n²) ≤ 1024²; negligible vs the embedding path it feeds. */
    for (int a = 1; a < n_cand; a++) {
        int idx = cand[a]; int b = a - 1;
        while (b >= 0) {
            const cos_sigma_distill_record_t *x = &st->ring[idx];
            const cos_sigma_distill_record_t *y = &st->ring[cand[b]];
            int better = (x->value >  y->value) ||
                         (x->value == y->value &&
                          x->insertion_order < y->insertion_order);
            if (!better) break;
            cand[b + 1] = cand[b];
            b--;
        }
        cand[b + 1] = idx;
    }

    int take = (n_cand < k) ? n_cand : k;
    for (int i = 0; i < take; i++) out_idx[i] = cand[i];
    *out_n = take;
    return COS_DISTILL_OK;
}

void cos_sigma_distill_stats(const cos_sigma_distill_state_t *st,
                             cos_sigma_distill_stats_t *out) {
    if (!out) return;
    if (!st) { memset(out, 0, sizeof *out); return; }
    *out = st->stats;
}

const cos_sigma_distill_record_t *
cos_sigma_distill_at(const cos_sigma_distill_state_t *st, int idx) {
    if (!st || idx < 0 || idx >= COS_DISTILL_RING_CAP) return NULL;
    const cos_sigma_distill_record_t *r = &st->ring[idx];
    return r->input ? r : NULL;
}

/* ------------------------------------------------------------------ *
 * Self-test                                                          *
 * ------------------------------------------------------------------ */

int cos_sigma_distill_self_test(void) {
    cos_sigma_distill_state_t st;
    if (cos_sigma_distill_init(&st, 0.0f) != COS_DISTILL_OK) return -1;
    if (fabsf(st.tau_teacher - 0.20f) > 1e-6f) return -2;

    /* Four pairs: gold, skip (low student σ), noise (both high),
     * skip (teacher not confident). */
    cos_sigma_distill_pair_t pairs[4] = {
        /* gold: student high, teacher low */
        { .input="q-gold",  .student_answer="A",
          .teacher_answer="A'", .student_sigma=0.80f, .teacher_sigma=0.05f,
          .timestamp_ns=1 },
        /* student was already confident, nothing to learn */
        { .input="q-ok",    .student_answer="B",
          .teacher_answer="B'", .student_sigma=0.05f, .teacher_sigma=0.05f,
          .timestamp_ns=2 },
        /* both high: teacher unreliable, skip */
        { .input="q-noise", .student_answer="C",
          .teacher_answer="C'", .student_sigma=0.90f, .teacher_sigma=0.90f,
          .timestamp_ns=3 },
        /* silver: moderate improvement, teacher confident */
        { .input="q-silver",.student_answer="D",
          .teacher_answer="D'", .student_sigma=0.60f, .teacher_sigma=0.10f,
          .timestamp_ns=4 },
    };
    for (int i = 0; i < 4; i++)
        if (cos_sigma_distill_append(&st, &pairs[i]) != COS_DISTILL_OK) return -3;

    /* filtered_in should be 3: q-gold, q-ok, q-silver (teacher_σ ≤ 0.2).
     * q-noise has teacher_σ 0.90 → filtered out. */
    if (st.stats.total_seen != 4)   return -4;
    if (st.stats.filtered_in != 3)  return -5;

    /* Select top 2 among student_σ ≥ 0.50; should be gold, then silver. */
    int idx[4]; int n = 0;
    if (cos_sigma_distill_select(&st, 2, 0.50f, idx, &n) != COS_DISTILL_OK) return -6;
    if (n != 2) return -7;
    const cos_sigma_distill_record_t *r0 = cos_sigma_distill_at(&st, idx[0]);
    const cos_sigma_distill_record_t *r1 = cos_sigma_distill_at(&st, idx[1]);
    if (!r0 || !r1) return -8;
    if (strcmp(r0->input, "q-gold")   != 0) return -9;
    if (strcmp(r1->input, "q-silver") != 0) return -10;
    if (!(r0->value > r1->value))           return -11;

    /* Bound checks: value in [-1, 1]. */
    if (r0->value < -1.0f || r0->value > 1.0f) return -12;

    /* Eviction check: fill the ring, ensure evicted counter rises. */
    for (int i = 0; i < COS_DISTILL_RING_CAP; i++) {
        cos_sigma_distill_pair_t p = {
            .input="q", .student_answer="s", .teacher_answer="t",
            .student_sigma=0.4f, .teacher_sigma=0.1f, .timestamp_ns=100+i };
        if (cos_sigma_distill_append(&st, &p) != COS_DISTILL_OK) return -13;
    }
    if (st.stats.evicted == 0) return -14;

    cos_sigma_distill_free(&st);
    return 0;
}
