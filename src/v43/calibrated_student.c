/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "calibrated_student.h"

#include <stddef.h>

float v43_calibration_loss(const sigma_decomposed_t *student_sigma, const sigma_decomposed_t *teacher_sigma)
{
    if (!student_sigma || !teacher_sigma) {
        return 0.0f;
    }
    float d_epistemic = student_sigma->epistemic - teacher_sigma->epistemic;
    float d_aleatoric = student_sigma->aleatoric - teacher_sigma->aleatoric;
    return d_epistemic * d_epistemic + d_aleatoric * d_aleatoric;
}

float v43_student_total_loss(float distill_loss, float calibration_loss, float lambda)
{
    return distill_loss + lambda * calibration_loss;
}
