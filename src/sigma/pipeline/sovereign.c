/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Sovereign primitive (zero-cloud accounting). */

#include "sovereign.h"

#include <math.h>
#include <string.h>

static double safe_eur(double e) {
    if (e != e || e == (1.0/0.0) || e == -(1.0/0.0)) return 0.0;
    if (e < 0.0) return 0.0;
    return e;
}

void cos_sigma_sovereign_init(cos_sigma_sovereign_t *s,
                              float min_sovereign_fraction)
{
    if (!s) return;
    memset(s, 0, sizeof *s);
    if (!(min_sovereign_fraction >= 0.0f
          && min_sovereign_fraction <= 1.0f))
        min_sovereign_fraction = 0.85f;
    s->min_sovereign_fraction = min_sovereign_fraction;
}

void cos_sigma_sovereign_record_local(cos_sigma_sovereign_t *s, double eur) {
    if (!s) return;
    ++s->n_local;
    s->eur_local_total += safe_eur(eur);
}

void cos_sigma_sovereign_record_cloud(cos_sigma_sovereign_t *s, double eur,
                                      uint64_t sent_bytes,
                                      uint64_t recv_bytes)
{
    if (!s) return;
    ++s->n_cloud;
    s->eur_cloud_total  += safe_eur(eur);
    s->bytes_sent_cloud += sent_bytes;
    s->bytes_recv_cloud += recv_bytes;
}

void cos_sigma_sovereign_record_abstain(cos_sigma_sovereign_t *s) {
    if (!s) return;
    ++s->n_abstain;
}

float cos_sigma_sovereign_fraction(const cos_sigma_sovereign_t *s) {
    if (!s) return 1.0f;
    uint64_t denom = (uint64_t)s->n_local + (uint64_t)s->n_cloud;
    if (denom == 0) return 1.0f;
    return (float)((double)s->n_local / (double)denom);
}

double cos_sigma_sovereign_eur_per_call(const cos_sigma_sovereign_t *s) {
    if (!s) return 0.0;
    uint64_t total = (uint64_t)s->n_local + (uint64_t)s->n_cloud
                   + (uint64_t)s->n_abstain;
    if (total == 0) return 0.0;
    return (s->eur_local_total + s->eur_cloud_total) / (double)total;
}

double cos_sigma_sovereign_monthly_eur_estimate(const cos_sigma_sovereign_t *s,
                                                int calls_per_day)
{
    if (!s) return 0.0;
    if (calls_per_day < 0) calls_per_day = 0;
    double per_call = cos_sigma_sovereign_eur_per_call(s);
    return per_call * (double)calls_per_day * 30.0;
}

cos_sovereign_verdict_t
cos_sigma_sovereign_verdict(const cos_sigma_sovereign_t *s)
{
    if (!s) return COS_SOVEREIGN_FULL;
    if (s->n_cloud == 0) return COS_SOVEREIGN_FULL;
    float frac = cos_sigma_sovereign_fraction(s);
    if (frac >= s->min_sovereign_fraction) return COS_SOVEREIGN_HYBRID;
    return COS_SOVEREIGN_DEPENDENT;
}

/* --- self-test --------------------------------------------------------- */

static int approx_eq_f(float a, float b, float tol) {
    float d = a - b;
    if (d < 0) d = -d;
    return d <= tol;
}

static int approx_eq_d(double a, double b, double tol) {
    double d = a - b;
    if (d < 0) d = -d;
    return d <= tol;
}

static int check_init_and_zero_state(void) {
    cos_sigma_sovereign_t s;
    cos_sigma_sovereign_init(&s, 0.90f);
    if (s.n_local != 0 || s.n_cloud != 0 || s.n_abstain != 0) return 10;
    if (s.eur_local_total != 0.0 || s.eur_cloud_total != 0.0) return 11;
    if (cos_sigma_sovereign_fraction(&s) != 1.0f) return 12;
    if (cos_sigma_sovereign_verdict(&s) != COS_SOVEREIGN_FULL) return 13;
    if (s.min_sovereign_fraction != 0.90f) return 14;
    /* Out-of-range threshold falls back to default 0.85. */
    cos_sigma_sovereign_init(&s, 1.5f);
    if (!approx_eq_f(s.min_sovereign_fraction, 0.85f, 1e-6f)) return 15;
    return 0;
}

static int check_full_sovereign(void) {
    cos_sigma_sovereign_t s;
    cos_sigma_sovereign_init(&s, 0.85f);
    for (int i = 0; i < 100; ++i)
        cos_sigma_sovereign_record_local(&s, 0.0001);   /* €0.0001/call */
    if (s.n_local != 100) return 20;
    if (cos_sigma_sovereign_verdict(&s) != COS_SOVEREIGN_FULL) return 21;
    if (cos_sigma_sovereign_fraction(&s) != 1.0f) return 22;
    if (!approx_eq_d(s.eur_local_total, 0.01, 1e-9)) return 23;
    return 0;
}

static int check_hybrid_85_15(void) {
    cos_sigma_sovereign_t s;
    cos_sigma_sovereign_init(&s, 0.85f);
    for (int i = 0; i < 85; ++i)
        cos_sigma_sovereign_record_local(&s, 0.0001);
    for (int i = 0; i < 15; ++i)
        cos_sigma_sovereign_record_cloud(&s, 0.012, 1024, 4096);
    if (cos_sigma_sovereign_verdict(&s) != COS_SOVEREIGN_HYBRID) return 30;
    if (!approx_eq_f(cos_sigma_sovereign_fraction(&s), 0.85f, 1e-6f)) return 31;
    if (!approx_eq_d(s.eur_cloud_total, 0.18, 1e-9)) return 32;
    if (s.bytes_sent_cloud != 15ULL * 1024) return 33;
    if (s.bytes_recv_cloud != 15ULL * 4096) return 34;
    /* eur per call = (0.0085 + 0.18) / 100 = 0.001885 */
    if (!approx_eq_d(cos_sigma_sovereign_eur_per_call(&s), 0.001885, 1e-9))
        return 35;
    return 0;
}

static int check_dependent_verdict(void) {
    cos_sigma_sovereign_t s;
    cos_sigma_sovereign_init(&s, 0.85f);
    for (int i = 0; i < 50; ++i)
        cos_sigma_sovereign_record_local(&s, 0.0);
    for (int i = 0; i < 50; ++i)
        cos_sigma_sovereign_record_cloud(&s, 0.0, 0, 0);
    /* fraction = 0.5 < 0.85 → DEPENDENT */
    if (cos_sigma_sovereign_verdict(&s) != COS_SOVEREIGN_DEPENDENT) return 40;
    return 0;
}

static int check_abstain_does_not_break_sovereignty(void) {
    cos_sigma_sovereign_t s;
    cos_sigma_sovereign_init(&s, 0.85f);
    cos_sigma_sovereign_record_abstain(&s);
    cos_sigma_sovereign_record_abstain(&s);
    /* No cloud calls → still FULL. */
    if (cos_sigma_sovereign_verdict(&s) != COS_SOVEREIGN_FULL) return 50;
    if (s.n_abstain != 2) return 51;
    if (cos_sigma_sovereign_fraction(&s) != 1.0f) return 52;
    return 0;
}

static int check_monthly_projection(void) {
    /* 85 local + 15 cloud calls/day baseline.  eur_per_call = 0.001885.
     * 100 calls/day × 30 days × 0.001885 = 5.655 €/month. */
    cos_sigma_sovereign_t s;
    cos_sigma_sovereign_init(&s, 0.85f);
    for (int i = 0; i < 85; ++i) cos_sigma_sovereign_record_local(&s, 0.0001);
    for (int i = 0; i < 15; ++i) cos_sigma_sovereign_record_cloud(&s, 0.012, 0, 0);
    double mon = cos_sigma_sovereign_monthly_eur_estimate(&s, 100);
    if (!approx_eq_d(mon, 5.655, 1e-3)) return 60;
    /* Negative calls_per_day clamped to 0. */
    if (cos_sigma_sovereign_monthly_eur_estimate(&s, -10) != 0.0) return 61;
    return 0;
}

static int check_nan_inf_eur_safe(void) {
    cos_sigma_sovereign_t s;
    cos_sigma_sovereign_init(&s, 0.85f);
    cos_sigma_sovereign_record_local(&s, 0.0/0.0);    /* NaN */
    cos_sigma_sovereign_record_cloud(&s, 1.0/0.0, 0, 0);   /* +inf */
    cos_sigma_sovereign_record_cloud(&s, -5.0, 0, 0);      /* negative */
    if (s.eur_local_total != 0.0) return 70;
    if (s.eur_cloud_total != 0.0) return 71;
    if (s.n_local != 1 || s.n_cloud != 2) return 72;
    return 0;
}

int cos_sigma_sovereign_self_test(void) {
    int rc;
    if ((rc = check_init_and_zero_state()))                 return rc;
    if ((rc = check_full_sovereign()))                      return rc;
    if ((rc = check_hybrid_85_15()))                        return rc;
    if ((rc = check_dependent_verdict()))                   return rc;
    if ((rc = check_abstain_does_not_break_sovereignty())) return rc;
    if ((rc = check_monthly_projection()))                  return rc;
    if ((rc = check_nan_inf_eur_safe()))                    return rc;
    return 0;
}
