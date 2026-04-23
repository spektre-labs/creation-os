/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#ifndef COS_SIGMA_SPEED_METRICS_H
#define COS_SIGMA_SPEED_METRICS_H

#ifdef __cplusplus
extern "C" {
#endif

struct cos_speed_metrics {
    int   tokens_generated;
    float wall_time_ms;
    float tokens_per_second;
    float ttft_ms;
};

struct cos_speed_metrics cos_speed_measure(const char *response,
                                           float       wall_ms);

/** When ttft_ms >= 0, sets ttft_ms field to TTFT (streaming).
 *  Otherwise leaves it at wall-clock echo from cos_speed_measure(). */
struct cos_speed_metrics cos_speed_measure_ex(const char *response,
                                              float       wall_ms,
                                              float       ttft_ms);

void cos_speed_print(const struct cos_speed_metrics *m);

#ifdef __cplusplus
}
#endif

#endif
