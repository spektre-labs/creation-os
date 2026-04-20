/* σ-Live — feedback-driven τ adaptation. */

#include "live.h"

#include <stdint.h>
#include <string.h>

static float clamp(float x, float lo, float hi) {
    if (x != x)   return lo;
    if (x < lo)   return lo;
    if (x > hi)   return hi;
    return x;
}

static uint16_t quant(float sigma) {
    float s = clamp(sigma, 0.0f, 1.0f) * 65535.0f + 0.5f;
    return (uint16_t)s;
}

static float dequant(uint16_t q) {
    return (float)q / 65535.0f;
}

int cos_sigma_live_init(cos_sigma_live_t *l,
                        cos_live_obs_t *buf, uint32_t capacity,
                        float seed_tau_accept, float seed_tau_rethink,
                        float tau_min, float tau_max,
                        float target_accept, float target_rethink,
                        uint32_t min_samples)
{
    if (!l || !buf)                        return -1;
    if (capacity == 0)                     return -2;
    if (!(tau_min >= 0.0f && tau_min <= 1.0f)) return -3;
    if (!(tau_max >= tau_min && tau_max <= 1.0f)) return -4;
    if (target_accept  <= 0.0f || target_accept  > 1.0f) return -5;
    if (target_rethink <= 0.0f || target_rethink > 1.0f) return -6;

    memset(l, 0, sizeof *l);
    memset(buf, 0, sizeof(cos_live_obs_t) * capacity);

    l->buf              = buf;
    l->capacity         = capacity;
    l->seed_tau_accept  = clamp(seed_tau_accept,  tau_min, tau_max);
    l->seed_tau_rethink = clamp(seed_tau_rethink, tau_min, tau_max);
    if (l->seed_tau_rethink < l->seed_tau_accept)
        l->seed_tau_rethink = l->seed_tau_accept;

    l->tau_accept       = l->seed_tau_accept;
    l->tau_rethink      = l->seed_tau_rethink;
    l->tau_min          = tau_min;
    l->tau_max          = tau_max;
    l->target_accept    = target_accept;
    l->target_rethink   = target_rethink;
    l->min_samples      = min_samples;
    return 0;
}

/* In-place insertion sort of a uint32_t permutation that keys on
 * obs[idx].sigma_q ascending.  N ≤ 256 in practice; O(N²) is fine
 * and avoids a heap allocation. */
static void sort_perm(const cos_live_obs_t *obs, uint32_t *idx,
                      uint32_t n)
{
    for (uint32_t i = 1; i < n; ++i) {
        uint32_t cur = idx[i];
        uint16_t key = obs[cur].sigma_q;
        int32_t  j   = (int32_t)i - 1;
        while (j >= 0 && obs[idx[j]].sigma_q > key) {
            idx[j + 1] = idx[j];
            --j;
        }
        idx[j + 1] = cur;
    }
}

void cos_sigma_live_adapt(cos_sigma_live_t *l) {
    if (!l) return;
    ++l->n_adapts;

    if (l->count < l->min_samples) {
        l->tau_accept  = l->seed_tau_accept;
        l->tau_rethink = l->seed_tau_rethink;
        l->accept_accuracy = 0.0f;
        l->rethink_accuracy = 0.0f;
        return;
    }

    /* Stack-allocated permutation (capped at 256). */
    uint32_t idx[256];
    uint32_t n = l->count;
    if (n > 256) n = 256;
    for (uint32_t i = 0; i < n; ++i) idx[i] = i;
    sort_perm(l->buf, idx, n);

    /* Sweep ascending σ; track cumulative correctness. */
    uint32_t correct = 0;
    float    tau_acc  = l->seed_tau_accept;
    float    tau_rth  = l->seed_tau_rethink;
    float    acc_at_acc  = 0.0f;
    float    acc_at_rth  = 0.0f;

    for (uint32_t i = 0; i < n; ++i) {
        const cos_live_obs_t *o = &l->buf[idx[i]];
        correct += (o->correct != 0) ? 1u : 0u;
        float acc = (float)correct / (float)(i + 1);
        float sig = dequant(o->sigma_q);

        if (acc >= l->target_accept) {
            tau_acc = sig;
            acc_at_acc = acc;
        }
        if (acc >= l->target_rethink) {
            tau_rth = sig;
            acc_at_rth = acc;
        }
    }

    /* Clamp and enforce ordering. */
    tau_acc = clamp(tau_acc, l->tau_min, l->tau_max);
    tau_rth = clamp(tau_rth, l->tau_min, l->tau_max);
    if (tau_rth < tau_acc) tau_rth = tau_acc;

    l->tau_accept      = tau_acc;
    l->tau_rethink     = tau_rth;
    l->accept_accuracy = acc_at_acc;
    l->rethink_accuracy = acc_at_rth;
}

void cos_sigma_live_observe(cos_sigma_live_t *l,
                            float sigma, bool correct)
{
    if (!l || !l->buf || l->capacity == 0) return;

    cos_live_obs_t o = {
        .sigma_q = quant(sigma),
        .correct = correct ? 1u : 0u,
        ._pad    = 0,
    };
    l->buf[l->head] = o;
    l->head = (l->head + 1u) % l->capacity;
    if (l->count < l->capacity) ++l->count;

    ++l->total_seen;
    if (correct) ++l->correct_seen;

    cos_sigma_live_adapt(l);
}

float cos_sigma_live_lifetime_accuracy(const cos_sigma_live_t *l) {
    if (!l || l->total_seen == 0) return 0.0f;
    return (float)((double)l->correct_seen / (double)l->total_seen);
}

/* --- self-test --------------------------------------------------------- */

static int check_init_guards(void) {
    cos_live_obs_t buf[4];
    cos_sigma_live_t l;
    if (cos_sigma_live_init(NULL, buf, 4, 0.2f, 0.5f,
                            0.0f, 1.0f, 0.95f, 0.50f, 2) >= 0) return 10;
    if (cos_sigma_live_init(&l, NULL, 4, 0.2f, 0.5f,
                            0.0f, 1.0f, 0.95f, 0.50f, 2) >= 0) return 11;
    if (cos_sigma_live_init(&l, buf, 0, 0.2f, 0.5f,
                            0.0f, 1.0f, 0.95f, 0.50f, 2) >= 0) return 12;
    if (cos_sigma_live_init(&l, buf, 4, 0.2f, 0.5f,
                            -0.1f, 1.0f, 0.95f, 0.50f, 2) >= 0) return 13;
    if (cos_sigma_live_init(&l, buf, 4, 0.2f, 0.5f,
                            0.0f, 1.0f, 1.5f, 0.50f, 2) >= 0) return 14;
    if (cos_sigma_live_init(&l, buf, 4, 0.2f, 0.5f,
                            0.0f, 1.0f, 0.95f, 1.5f, 2) >= 0) return 15;
    return 0;
}

static int check_seed_until_min_samples(void) {
    cos_live_obs_t buf[32];
    cos_sigma_live_t l;
    int rc = cos_sigma_live_init(&l, buf, 32, 0.20f, 0.50f,
                                 0.0f, 1.0f, 0.95f, 0.50f, 8);
    if (rc != 0) return 20;
    /* 4 observations, min_samples=8 → τs stay at seeds. */
    for (int i = 0; i < 4; ++i)
        cos_sigma_live_observe(&l, 0.3f, true);
    if (l.tau_accept  != 0.20f) return 21;
    if (l.tau_rethink != 0.50f) return 22;
    if (l.count       != 4)     return 23;
    return 0;
}

static int check_all_correct_low_sigma_liberalises(void) {
    /* All correct over σ = 0.1..0.4 → accept threshold should RISE
     * toward the highest σ at which we're still 95% correct. */
    cos_live_obs_t buf[32];
    cos_sigma_live_t l;
    cos_sigma_live_init(&l, buf, 32, 0.20f, 0.50f,
                        0.0f, 1.0f, 0.95f, 0.50f, 4);
    /* Interleave σ in 0.1, 0.2, 0.3, 0.4 → all correct. */
    float xs[] = {0.10f, 0.20f, 0.30f, 0.40f,
                  0.15f, 0.25f, 0.35f, 0.40f};
    for (size_t i = 0; i < sizeof xs / sizeof xs[0]; ++i)
        cos_sigma_live_observe(&l, xs[i], true);
    /* Cumulative accuracy is 100% at every step → τ_accept picks
     * the LARGEST observed σ, which is 0.40 (clamped ≤ tau_max). */
    if (l.tau_accept < 0.39f || l.tau_accept > 0.41f) return 30;
    /* τ_rethink also at the max observed σ. */
    if (l.tau_rethink < l.tau_accept) return 31;
    return 0;
}

static int check_wrong_at_low_sigma_tightens(void) {
    /* If everything is WRONG, cumulative accuracy is always 0 →
     * τ_accept falls back to seed (since neither target is met
     * during the sweep). */
    cos_live_obs_t buf[16];
    cos_sigma_live_t l;
    cos_sigma_live_init(&l, buf, 16, 0.60f, 0.80f,
                        0.0f, 1.0f, 0.95f, 0.50f, 4);
    for (int i = 0; i < 8; ++i)
        cos_sigma_live_observe(&l, 0.1f + 0.05f * (float)i, false);
    /* No cumulative bin ≥ 0.95 or ≥ 0.50 → τs stay at seed
     * (the algorithm preserves them when no bin qualifies). */
    if (l.tau_accept  != 0.60f) return 40;
    if (l.tau_rethink != 0.80f) return 41;
    /* Lifetime accuracy = 0. */
    if (cos_sigma_live_lifetime_accuracy(&l) != 0.0f) return 42;
    return 0;
}

static int check_mixed_curve(void) {
    /* 10 observations: σ ascending, correctness matches boundary 0.5.
     * σ = [0.05, 0.10, 0.15, 0.20, 0.25, 0.55, 0.60, 0.65, 0.70, 0.75]
     * correct = [T, T, T, T, T, F, F, F, F, F]
     * Cumulative accuracy at the 5th (σ=0.25) = 5/5 = 1.00 → τ_accept
     * = 0.25 (last bin where acc ≥ 0.95).  Acc at 6 = 5/6 ≈ 0.83 ≥
     * 0.50 → τ_rethink = 0.55.  Acc at 7 = 5/7 ≈ 0.71 ≥ 0.50 →
     * τ_rethink = 0.60.  Acc at 8 = 5/8 = 0.625 ≥ 0.50 → τ_rethink
     * = 0.65.  Acc at 9 = 5/9 ≈ 0.556 ≥ 0.50 → τ_rethink = 0.70.
     * Acc at 10 = 5/10 = 0.50 ≥ 0.50 → τ_rethink = 0.75. */
    cos_live_obs_t buf[16];
    cos_sigma_live_t l;
    cos_sigma_live_init(&l, buf, 16, 0.30f, 0.60f,
                        0.0f, 1.0f, 0.95f, 0.50f, 4);
    float sigs[10] = {0.05f, 0.10f, 0.15f, 0.20f, 0.25f,
                      0.55f, 0.60f, 0.65f, 0.70f, 0.75f};
    bool  cors[10] = { true,  true,  true,  true,  true,
                      false, false, false, false, false};
    for (int i = 0; i < 10; ++i)
        cos_sigma_live_observe(&l, sigs[i], cors[i]);

    if (l.tau_accept  < 0.24f || l.tau_accept  > 0.26f) return 50;
    if (l.tau_rethink < 0.74f || l.tau_rethink > 0.76f) return 51;
    if (cos_sigma_live_lifetime_accuracy(&l) < 0.499f ||
        cos_sigma_live_lifetime_accuracy(&l) > 0.501f) return 52;
    return 0;
}

static int check_monotone_invariant(void) {
    /* τ_accept ≤ τ_rethink always. */
    cos_live_obs_t buf[64];
    cos_sigma_live_t l;
    cos_sigma_live_init(&l, buf, 64, 0.30f, 0.60f,
                        0.0f, 1.0f, 0.95f, 0.50f, 4);
    uint32_t state = 0xB0000001u;
    for (int i = 0; i < 200; ++i) {
        state = state * 1664525u + 1013904223u;
        float sig = (float)((state >> 8) & 0xFFFF) / 65535.0f;
        state = state * 1664525u + 1013904223u;
        bool ok = ((state >> 8) & 1u) != 0u;
        cos_sigma_live_observe(&l, sig, ok);
        if (l.tau_rethink < l.tau_accept) return 60;
        if (l.tau_accept  < 0.0f || l.tau_accept  > 1.0f) return 61;
        if (l.tau_rethink < 0.0f || l.tau_rethink > 1.0f) return 62;
    }
    /* Ring buffer wraps: count == capacity after > cap observations. */
    if (l.count != 64) return 63;
    /* Lifetime totals keep counting past the ring size. */
    if (l.total_seen != 200) return 64;
    return 0;
}

static int check_lifetime_accuracy(void) {
    cos_live_obs_t buf[8];
    cos_sigma_live_t l;
    cos_sigma_live_init(&l, buf, 8, 0.30f, 0.60f,
                        0.0f, 1.0f, 0.95f, 0.50f, 4);
    /* 3 correct + 7 wrong → lifetime = 0.30. */
    for (int i = 0; i < 3; ++i) cos_sigma_live_observe(&l, 0.2f, true);
    for (int i = 0; i < 7; ++i) cos_sigma_live_observe(&l, 0.8f, false);
    float a = cos_sigma_live_lifetime_accuracy(&l);
    if (a < 0.299f || a > 0.301f) return 70;
    if (l.total_seen   != 10) return 71;
    if (l.correct_seen !=  3) return 72;
    return 0;
}

int cos_sigma_live_self_test(void) {
    int rc;
    if ((rc = check_init_guards()))                   return rc;
    if ((rc = check_seed_until_min_samples()))        return rc;
    if ((rc = check_all_correct_low_sigma_liberalises())) return rc;
    if ((rc = check_wrong_at_low_sigma_tightens()))   return rc;
    if ((rc = check_mixed_curve()))                   return rc;
    if ((rc = check_monotone_invariant()))            return rc;
    if ((rc = check_lifetime_accuracy()))             return rc;
    return 0;
}
