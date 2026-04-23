/*
 * Awareness time series — SQLite log of consciousness_index samples.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_AWARENESS_LOG_H
#define COS_SIGMA_AWARENESS_LOG_H

#include "consciousness_meter.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_awareness_entry {
    float   consciousness_index;
    int     level;
    float   phi_proxy;
    int64_t timestamp_ms;
};

int cos_awareness_open(void);

void cos_awareness_close(void);

int cos_awareness_log(const struct cos_consciousness_state *cs);

int cos_awareness_trend(float *trend, int *n_entries);

int cos_awareness_report(char *report, int max_len);

int cos_awareness_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_AWARENESS_LOG_H */
