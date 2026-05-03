/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * σ-gate for raw sensor integers: deviation from baseline → Q16 σ.
 */
#include "sigma_sensor.h"

static int32_t compute_sensor_sigma_q16(sigma_sensor_t *s, int32_t reading)
{
    int64_t diff;
    int64_t num;
    int32_t sg;

    if (!s || s->tolerance <= 0)
        return SIGMA_TINY_Q16 - 1;

    diff = (int64_t)reading - (int64_t)s->baseline;
    if (diff < 0)
        diff = -diff;

    num = diff * (int64_t)SIGMA_TINY_Q16;
    sg = (int32_t)(num / (int64_t)s->tolerance);
    if (sg < 0)
        sg = 0;
    if (sg > SIGMA_TINY_Q16 - 1)
        sg = SIGMA_TINY_Q16 - 1;
    return sg;
}

void sigma_sensor_init(sigma_sensor_t *s, int32_t baseline, int32_t tolerance)
{
    if (!s)
        return;
    s->baseline  = baseline;
    s->tolerance = tolerance;
    s->anomalies = 0;
    sigma_state_init(&s->gate);
}

sigma_verdict_t sigma_sensor_step(sigma_sensor_t *s, int32_t reading)
{
    int32_t         sg;
    sigma_verdict_t v;

    if (!s)
        return SIGMA_ABSTAIN;

    sg = compute_sensor_sigma_q16(s, reading);
    sigma_update(&s->gate, sg, SIGMA_Q16(0.9));
    v = sigma_gate(&s->gate);
    if (v == SIGMA_ABSTAIN)
        s->anomalies++;
    return v;
}
