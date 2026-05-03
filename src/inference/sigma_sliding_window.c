/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "cos_sigma_mirror.h"
#include "sigma_sliding_window.h"

int32_t cos_sigma_sliding_window_size(const cos_sigma_sliding_window_t *sw,
                                     int32_t current_sigma_q16)
{
    int32_t scale;
    int32_t range;
    int32_t window;
    int64_t w64;
    if (!sw)
        return 0;
    if (current_sigma_q16 < 0)
        current_sigma_q16 = 0;
    if (current_sigma_q16 > COS_SIGMA_Q16_ONE - 1)
        current_sigma_q16 = COS_SIGMA_Q16_ONE - 1;
    /* Low σ → large window: scale = 1 - σ in Q16 */
    scale = COS_SIGMA_Q16_ONE - current_sigma_q16;
    range = sw->max_window - sw->min_window;
    if (range < 0)
        range = 0;
    w64   = (int64_t)sw->min_window + (((int64_t)range * scale) >> 16);
    window = (int32_t)w64;
    if (window < sw->min_window)
        window = sw->min_window;
    if (window > sw->max_window)
        window = sw->max_window;
    return window;
}
