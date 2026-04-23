/*
 * Coherence watchdog — σ_mean + k_eff monitor for long-running missions.
 *
 * Distinct symbols from pipeline/watchdog.c (ring-buffer σ trend kernel).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_COHERENCE_WATCHDOG_H
#define COS_SIGMA_COHERENCE_WATCHDOG_H

#include "state_ledger.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cos_cw_config {
    float k_eff_min;
    float sigma_max;
    int   check_interval_ms;
    int   alarm_count;
};

void cos_cw_default_config(struct cos_cw_config *out);

void cos_cw_install_config(const struct cos_cw_config *config);

int cos_cw_start(const struct cos_cw_config *config);

/** Returns 1 if watchdog would fire PAUSE (consecutive alarm threshold met). */
int cos_cw_check(const struct cos_state_ledger *ledger);

int cos_cw_stop(void);

int cos_cw_active(void);

int cos_cw_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_COHERENCE_WATCHDOG_H */
