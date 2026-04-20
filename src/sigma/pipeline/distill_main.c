/* σ-Distill self-test + canonical demo.
 *
 * Simulates a 25-pair escalation stream covering the three value
 * regimes (gold / skip-student-sure / skip-teacher-unsure), then
 * emits pinned JSON showing:
 *   - total_seen, filtered_in, evicted, top-K picks
 *   - top-K strictly sorted by σ_distill_value desc
 *   - before/after "escalation rate" regime:
 *         escalation_rate_before = student_σ > τ_escalate mean
 *         escalation_rate_after  = (student_σ - Δ) > τ_escalate mean
 *     where Δ is the σ-credit per filtered pair (proxy for the
 *     improvement TTT will actually deliver).
 *
 * Pinned by benchmarks/sigma_pipeline/check_sigma_distill.sh.
 */

#include "distill.h"

#include <stdio.h>
#include <string.h>

/* Deterministic synthetic stream.  Three regimes: gold (g), skip-
 * student-sure (o), noise (n).  25 pairs by construction. */
static const struct demo_pair {
    const char *input;
    float       student_sigma;
    float       teacher_sigma;
} DEMO[] = {
    {"q-gold-01",  0.82f, 0.05f},
    {"q-ok-02",    0.08f, 0.07f},
    {"q-noise-03", 0.88f, 0.85f},
    {"q-gold-04",  0.76f, 0.10f},
    {"q-silver-05",0.55f, 0.12f},
    {"q-ok-06",    0.05f, 0.05f},
    {"q-gold-07",  0.91f, 0.03f},
    {"q-noise-08", 0.70f, 0.80f},
    {"q-silver-09",0.60f, 0.15f},
    {"q-gold-10",  0.85f, 0.02f},
    {"q-ok-11",    0.10f, 0.08f},
    {"q-bronze-12",0.40f, 0.18f},
    {"q-gold-13",  0.80f, 0.06f},
    {"q-silver-14",0.62f, 0.11f},
    {"q-noise-15", 0.95f, 0.75f},
    {"q-gold-16",  0.88f, 0.04f},
    {"q-silver-17",0.58f, 0.14f},
    {"q-ok-18",    0.03f, 0.02f},
    {"q-bronze-19",0.42f, 0.17f},
    {"q-gold-20",  0.79f, 0.07f},
    {"q-noise-21", 0.90f, 0.92f},
    {"q-silver-22",0.65f, 0.09f},
    {"q-gold-23",  0.83f, 0.01f},
    {"q-bronze-24",0.45f, 0.16f},
    {"q-silver-25",0.56f, 0.13f},
};

static const int DEMO_N = (int)(sizeof DEMO / sizeof DEMO[0]);

static int json_escape(const char *s, char *dst, int cap) {
    int j = 0;
    if (!s || cap <= 2) { if (cap > 0) dst[0] = '\0'; return 0; }
    for (int i = 0; s[i] && j + 2 < cap; i++) {
        char c = s[i];
        if (c == '\\' || c == '"') {
            if (j + 3 >= cap) break;
            dst[j++] = '\\';
            dst[j++] = c;
        } else if (c >= 0 && c < 0x20) {
            if (j + 7 >= cap) break;
            j += snprintf(dst + j, cap - j, "\\u%04x", (unsigned)c);
        } else {
            dst[j++] = c;
        }
    }
    dst[j] = '\0';
    return j;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int rc = cos_sigma_distill_self_test();

    cos_sigma_distill_state_t st;
    cos_sigma_distill_init(&st, /*tau=*/0.20f);

    const float TAU_ESCALATE = 0.30f;
    float pre_escalate = 0.0f;

    for (int i = 0; i < DEMO_N; i++) {
        cos_sigma_distill_pair_t p = {
            .input           = DEMO[i].input,
            .student_answer  = "student",
            .student_sigma   = DEMO[i].student_sigma,
            .teacher_answer  = "teacher",
            .teacher_sigma   = DEMO[i].teacher_sigma,
            .timestamp_ns    = (uint64_t)(i + 1) * 1000,
        };
        if (DEMO[i].student_sigma > TAU_ESCALATE) pre_escalate += 1.0f;
        cos_sigma_distill_append(&st, &p);
    }
    pre_escalate /= (float)DEMO_N;

    int idx[8]; int n = 0;
    cos_sigma_distill_select(&st, 5, /*floor=*/0.30f, idx, &n);

    /* Predicted post-distill escalation rate.  The student, taught
     * on the top-K σ-selected pairs, converges from its own σ
     * toward the teacher's σ by fraction ALPHA (asymptotic limit
     * of repeated TTT updates on the same pair):
     *     post_σ = (1-α)·student_σ + α·teacher_σ
     * α = 0.90 models a student that nearly matches the teacher on
     * the taught pairs and leaves untaught pairs unchanged.  This
     * is a *demo* projection; real TTT numbers live in
     * benchmarks/pipeline/truthfulqa_results.md. */
    const float ALPHA_LEARN = 0.90f;
    int taught[COS_DISTILL_RING_CAP] = {0};
    for (int i = 0; i < n; i++) taught[idx[i]] = 1;

    float post_escalate = 0.0f;
    for (int slot = 0; slot < COS_DISTILL_RING_CAP; slot++) {
        const cos_sigma_distill_record_t *r = cos_sigma_distill_at(&st, slot);
        if (!r) continue;
        float s = r->student_sigma;
        if (taught[slot]) {
            s = (1.0f - ALPHA_LEARN) * r->student_sigma
              +         ALPHA_LEARN  * r->teacher_sigma;
            if (s < 0.0f) s = 0.0f;
        }
        if (s > TAU_ESCALATE) post_escalate += 1.0f;
    }
    post_escalate /= (float)DEMO_N;

    cos_sigma_distill_stats_t snap;
    cos_sigma_distill_stats(&st, &snap);

    printf("{\"kernel\":\"sigma_distill\","
           "\"self_test_rc\":%d,"
           "\"tau_teacher\":%.4f,"
           "\"tau_escalate\":%.4f,"
           "\"total_seen\":%llu,"
           "\"filtered_in\":%llu,"
           "\"evicted\":%llu,"
           "\"mean_value_filt\":%.4f,"
           "\"mean_student_filt\":%.4f,"
           "\"mean_teacher_filt\":%.4f,"
           "\"top_k\":[",
           rc,
           (double)st.tau_teacher,
           (double)TAU_ESCALATE,
           (unsigned long long)snap.total_seen,
           (unsigned long long)snap.filtered_in,
           (unsigned long long)snap.evicted,
           (double)snap.mean_value_filt,
           (double)snap.mean_student_filt,
           (double)snap.mean_teacher_filt);

    char escaped[256];
    for (int i = 0; i < n; i++) {
        const cos_sigma_distill_record_t *r = cos_sigma_distill_at(&st, idx[i]);
        json_escape(r ? r->input : "", escaped, sizeof escaped);
        printf("%s{\"rank\":%d,\"input\":\"%s\","
               "\"student_sigma\":%.4f,\"teacher_sigma\":%.4f,"
               "\"value\":%.4f}",
               i ? "," : "", i + 1, escaped,
               (double)(r ? r->student_sigma : 0.0f),
               (double)(r ? r->teacher_sigma : 0.0f),
               (double)(r ? r->value         : 0.0f));
    }

    printf("],\"escalation\":{"
             "\"before\":%.4f,"
             "\"after_modeled\":%.4f,"
             "\"delta\":%.4f"
           "},"
           "\"pass\":%s}\n",
           (double)pre_escalate,
           (double)post_escalate,
           (double)(pre_escalate - post_escalate),
           rc == 0 ? "true" : "false");

    cos_sigma_distill_free(&st);
    return rc == 0 ? 0 : 1;
}
