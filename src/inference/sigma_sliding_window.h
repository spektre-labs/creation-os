/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * σ-shaped sliding context window (integer σ in Q16.16).
 */
#ifndef SIGMA_SLIDING_WINDOW_H
#define SIGMA_SLIDING_WINDOW_H

#include <stdint.h>

typedef struct {
    int32_t base_window; /* typical fixed window when σ is “neutral” */
    int32_t min_window;
    int32_t max_window;
} cos_sigma_sliding_window_t;

int32_t cos_sigma_sliding_window_size(const cos_sigma_sliding_window_t *sw,
                                     int32_t current_sigma_q16);

#endif /* SIGMA_SLIDING_WINDOW_H */
