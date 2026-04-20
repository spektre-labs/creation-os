/*
 * σ-Sovereign — zero-cloud accounting (v264 sovereign stack).
 *
 * The previous 19 primitives can run end-to-end without any
 * cloud-side dependency.  σ-Sovereign records the *fact* of that
 * sovereignty: every call into the σ-pipeline declares whether
 * it stayed local or escalated to a third-party API.
 *
 * Counters:
 *   - n_local           : local model calls (e.g. BitNet-2B)
 *   - n_cloud           : escalations (Claude / GPT / DeepSeek)
 *   - n_abstain         : the σ-gate refused to answer
 *   - eur_local_total   : projected local cost (electricity)
 *   - eur_cloud_total   : sum of API invoice cents
 *   - bytes_sent_cloud  : total payload sent off-device
 *   - bytes_recv_cloud  : total payload received from cloud
 *
 * Derived:
 *   sovereign_fraction = n_local / (n_local + n_cloud)         ∈ [0, 1]
 *   eur_per_call       = (eur_local_total + eur_cloud_total)
 *                          / max(1, n_local + n_cloud + n_abstain)
 *
 * Verdict:
 *   COS_SOVEREIGN_FULL    = no cloud calls, ever
 *   COS_SOVEREIGN_HYBRID  = cloud calls present but fraction
 *                           ≥ ``min_sovereign_fraction``
 *   COS_SOVEREIGN_DEPENDENT = below the threshold; "Creation OS is
 *                             not sovereign on this device"
 *
 * The primitive does not perform network calls itself; it is a
 * truthful ledger that the orchestrator updates.  The default
 * monthly projection (`monthly_eur_estimate`) extrapolates the
 * recorded average cost-per-call to a 30-day window, scaled by
 * the user-supplied calls-per-day budget.
 *
 * Contracts (v0):
 *   1. record_local()  : eur ≥ 0 → bumps n_local + eur_local_total.
 *   2. record_cloud()  : eur ≥ 0, bytes ≥ 0 → bumps n_cloud +
 *                        eur_cloud_total + bytes_sent + bytes_recv.
 *   3. record_abstain(): bumps n_abstain only.
 *   4. NaN / inf eur values are treated as zero.
 *   5. verdict() with zero calls returns COS_SOVEREIGN_FULL (no
 *      cloud calls means full sovereignty).
 *   6. monthly_eur_estimate(calls_per_day): integrates over 30
 *      days using the recorded eur_per_call.  calls_per_day < 0
 *      clamped to 0.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SOVEREIGN_H
#define COS_SIGMA_SOVEREIGN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_SOVEREIGN_DEPENDENT = 0,
    COS_SOVEREIGN_HYBRID    = 1,
    COS_SOVEREIGN_FULL      = 2,
} cos_sovereign_verdict_t;

typedef struct {
    uint32_t n_local;
    uint32_t n_cloud;
    uint32_t n_abstain;
    double   eur_local_total;
    double   eur_cloud_total;
    uint64_t bytes_sent_cloud;
    uint64_t bytes_recv_cloud;
    float    min_sovereign_fraction;   /* default 0.85 */
} cos_sigma_sovereign_t;

void  cos_sigma_sovereign_init(cos_sigma_sovereign_t *s,
                               float min_sovereign_fraction);

void  cos_sigma_sovereign_record_local  (cos_sigma_sovereign_t *s, double eur);
void  cos_sigma_sovereign_record_cloud  (cos_sigma_sovereign_t *s, double eur,
                                         uint64_t sent_bytes,
                                         uint64_t recv_bytes);
void  cos_sigma_sovereign_record_abstain(cos_sigma_sovereign_t *s);

float cos_sigma_sovereign_fraction(const cos_sigma_sovereign_t *s);
double cos_sigma_sovereign_eur_per_call(const cos_sigma_sovereign_t *s);
double cos_sigma_sovereign_monthly_eur_estimate(const cos_sigma_sovereign_t *s,
                                                int calls_per_day);

cos_sovereign_verdict_t
      cos_sigma_sovereign_verdict(const cos_sigma_sovereign_t *s);

int   cos_sigma_sovereign_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_SOVEREIGN_H */
