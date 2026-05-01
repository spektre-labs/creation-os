/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Agent primitive (OODA loop + autonomy gate). */

#include "agent.h"

#include <math.h>
#include <string.h>

static float clamp01(float x) {
    if (x != x)   return 1.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

float cos_sigma_agent_min_autonomy(cos_action_class_t cls) {
    switch (cls) {
        case COS_ACT_READ:         return 0.30f;
        case COS_ACT_WRITE:        return 0.60f;
        case COS_ACT_NETWORK:      return 0.80f;
        case COS_ACT_IRREVERSIBLE: return 0.95f;
        default:                   return 1.01f;  /* unknown = BLOCK */
    }
}

int cos_sigma_agent_init(cos_sigma_agent_t *a,
                         float base_autonomy, float confirm_margin)
{
    if (!a) return -1;
    if (!(base_autonomy >= 0.0f && base_autonomy <= 1.0f)) return -2;
    if (!(confirm_margin > 0.0f && confirm_margin < 1.0f)) return -3;
    memset(a, 0, sizeof *a);
    a->base_autonomy   = base_autonomy;
    a->confirm_margin  = confirm_margin;
    a->phase           = COS_OODA_OBSERVE;
    return 0;
}

int cos_sigma_agent_advance(cos_sigma_agent_t *a,
                            cos_ooda_phase_t expected)
{
    if (!a) return -1;
    if (a->phase != expected) return -1;
    if (a->phase == COS_OODA_REFLECT) {
        a->phase = COS_OODA_OBSERVE;
        ++a->n_cycles;
    } else {
        a->phase = (cos_ooda_phase_t)((int)a->phase + 1);
    }
    return 0;
}

cos_agent_decision_t
cos_sigma_agent_gate(const cos_sigma_agent_t *a,
                     cos_action_class_t cls, float sigma)
{
    if (!a) return COS_AGENT_BLOCK;
    if (sigma != sigma) return COS_AGENT_BLOCK;
    float s   = clamp01(sigma);
    float eff = a->base_autonomy * (1.0f - s);
    float req = cos_sigma_agent_min_autonomy(cls);
    if (req > 1.0f) return COS_AGENT_BLOCK;
    if (eff >= req) return COS_AGENT_ALLOW;
    if (eff >= req - a->confirm_margin) return COS_AGENT_CONFIRM;
    return COS_AGENT_BLOCK;
}

void cos_sigma_agent_reflect(cos_sigma_agent_t *a,
                             float sigma_pred, float sigma_obs)
{
    if (!a) return;
    float p = clamp01(sigma_pred);
    float o = clamp01(sigma_obs);
    a->sum_sigma_pred += p;
    a->sum_sigma_obs  += o;
    a->sum_abs_err    += fabs((double)p - (double)o);
    a->last_sigma      = o;
}

float cos_sigma_agent_calibration_err(const cos_sigma_agent_t *a) {
    if (!a || a->n_cycles <= 0) return 0.0f;
    return (float)(a->sum_abs_err / (double)a->n_cycles);
}

/* --- self-test --------------------------------------------------------- */

static int check_init_guards(void) {
    cos_sigma_agent_t a;
    if (cos_sigma_agent_init(NULL, 0.5f, 0.1f) >= 0) return 10;
    if (cos_sigma_agent_init(&a, -0.1f, 0.1f) >= 0) return 11;
    if (cos_sigma_agent_init(&a, 1.1f, 0.1f) >= 0) return 12;
    if (cos_sigma_agent_init(&a, 0.5f, 0.0f) >= 0) return 13;
    if (cos_sigma_agent_init(&a, 0.5f, 1.0f) >= 0) return 14;
    return 0;
}

static int check_full_cycle(void) {
    cos_sigma_agent_t a;
    cos_sigma_agent_init(&a, 0.8f, 0.1f);
    if (a.phase != COS_OODA_OBSERVE) return 20;
    if (cos_sigma_agent_advance(&a, COS_OODA_OBSERVE) != 0) return 21;
    if (a.phase != COS_OODA_ORIENT) return 22;
    if (cos_sigma_agent_advance(&a, COS_OODA_ORIENT) != 0) return 23;
    if (cos_sigma_agent_advance(&a, COS_OODA_DECIDE) != 0) return 24;
    if (cos_sigma_agent_advance(&a, COS_OODA_ACT) != 0) return 25;
    if (cos_sigma_agent_advance(&a, COS_OODA_REFLECT) != 0) return 26;
    if (a.phase != COS_OODA_OBSERVE) return 27;
    if (a.n_cycles != 1) return 28;
    /* Out-of-sequence call must be rejected. */
    if (cos_sigma_agent_advance(&a, COS_OODA_DECIDE) != -1) return 29;
    return 0;
}

static int check_low_sigma_allow(void) {
    /* base = 1.0, σ = 0.05  →  effective = 0.95.
     * READ (0.30), WRITE (0.60), NETWORK (0.80) all ALLOW.
     * IRREVERSIBLE (0.95) ALLOW (= boundary).            */
    cos_sigma_agent_t a;
    cos_sigma_agent_init(&a, 1.0f, 0.1f);
    if (cos_sigma_agent_gate(&a, COS_ACT_READ,  0.05f) != COS_AGENT_ALLOW) return 30;
    if (cos_sigma_agent_gate(&a, COS_ACT_WRITE, 0.05f) != COS_AGENT_ALLOW) return 31;
    if (cos_sigma_agent_gate(&a, COS_ACT_NETWORK, 0.05f) != COS_AGENT_ALLOW) return 32;
    if (cos_sigma_agent_gate(&a, COS_ACT_IRREVERSIBLE, 0.05f) != COS_AGENT_ALLOW) return 33;
    return 0;
}

static int check_high_sigma_block(void) {
    /* base = 1.0, σ = 0.95  →  effective = 0.05.
     * Even READ (0.30) requires effective ≥ 0.30 → BLOCK.
     * confirm_margin = 0.10 → CONFIRM band [0.20, 0.30) → still
     * BLOCK at 0.05.                                          */
    cos_sigma_agent_t a;
    cos_sigma_agent_init(&a, 1.0f, 0.1f);
    for (int c = 0; c < COS_ACT__N; ++c)
        if (cos_sigma_agent_gate(&a, (cos_action_class_t)c, 0.95f)
                != COS_AGENT_BLOCK) return 40 + c;
    return 0;
}

static int check_confirm_band(void) {
    /* base = 1.0, σ = 0.45  →  effective = 0.55.
     * WRITE requires 0.60.  margin 0.10 → band [0.50, 0.60) →
     * 0.55 lands in CONFIRM.  NETWORK requires 0.80 → BLOCK. */
    cos_sigma_agent_t a;
    cos_sigma_agent_init(&a, 1.0f, 0.1f);
    if (cos_sigma_agent_gate(&a, COS_ACT_WRITE,   0.45f) != COS_AGENT_CONFIRM) return 50;
    if (cos_sigma_agent_gate(&a, COS_ACT_NETWORK, 0.45f) != COS_AGENT_BLOCK)   return 51;
    if (cos_sigma_agent_gate(&a, COS_ACT_READ,    0.45f) != COS_AGENT_ALLOW)   return 52;
    return 0;
}

static int check_gate_guards(void) {
    cos_sigma_agent_t a;
    cos_sigma_agent_init(&a, 1.0f, 0.1f);
    /* NULL agent → BLOCK. */
    if (cos_sigma_agent_gate(NULL, COS_ACT_READ, 0.0f) != COS_AGENT_BLOCK) return 60;
    /* NaN σ → BLOCK. */
    float nan_v = 0.0f / 0.0f;
    if (cos_sigma_agent_gate(&a, COS_ACT_READ, nan_v) != COS_AGENT_BLOCK) return 61;
    /* Unknown action class → BLOCK. */
    if (cos_sigma_agent_gate(&a, (cos_action_class_t)999, 0.0f)
            != COS_AGENT_BLOCK) return 62;
    return 0;
}

static int check_reflect_calibration(void) {
    cos_sigma_agent_t a;
    cos_sigma_agent_init(&a, 0.8f, 0.1f);
    /* Run 4 OODA cycles with predicted vs observed σ pairs. */
    float pairs[4][2] = {
        {0.10f, 0.20f},   /* err 0.10 */
        {0.30f, 0.30f},   /* err 0.00 */
        {0.50f, 0.30f},   /* err 0.20 */
        {0.10f, 0.10f},   /* err 0.00 */
    };
    for (int i = 0; i < 4; ++i) {
        cos_sigma_agent_advance(&a, COS_OODA_OBSERVE);
        cos_sigma_agent_advance(&a, COS_OODA_ORIENT);
        cos_sigma_agent_advance(&a, COS_OODA_DECIDE);
        cos_sigma_agent_advance(&a, COS_OODA_ACT);
        cos_sigma_agent_reflect(&a, pairs[i][0], pairs[i][1]);
        cos_sigma_agent_advance(&a, COS_OODA_REFLECT);
    }
    if (a.n_cycles != 4) return 70;
    float ce = cos_sigma_agent_calibration_err(&a);
    /* Expected mean |err| = (0.10+0.00+0.20+0.00)/4 = 0.075 */
    if (fabsf(ce - 0.075f) > 1e-5f) return 71;
    return 0;
}

int cos_sigma_agent_self_test(void) {
    int rc;
    if ((rc = check_init_guards()))         return rc;
    if ((rc = check_full_cycle()))          return rc;
    if ((rc = check_low_sigma_allow()))     return rc;
    if ((rc = check_high_sigma_block()))    return rc;
    if ((rc = check_confirm_band()))        return rc;
    if ((rc = check_gate_guards()))         return rc;
    if ((rc = check_reflect_calibration())) return rc;
    return 0;
}
