/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v34: Platt-style logistic map from a scalar σ feature to P(correct) (lab).
 */
#ifndef CREATION_OS_SIGMA_CALIBRATE_H
#define CREATION_OS_SIGMA_CALIBRATE_H

typedef struct {
    float a;
    float b;
    int valid;
} sigma_platt_params_t;

float sigma_platt_p_correct(float sigma_raw, float a, float b);

/** Load {"platt_a":..,"platt_b":..} from JSON path; defaults a=-1,b=0 on missing. */
int sigma_platt_load(const char *path, sigma_platt_params_t *out);

#endif /* CREATION_OS_SIGMA_CALIBRATE_H */
