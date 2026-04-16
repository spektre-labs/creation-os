/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v43 lab: dual objective — distillation + σ calibration (student matches teacher UQ).
 */
#ifndef CREATION_OS_V43_CALIBRATED_STUDENT_H
#define CREATION_OS_V43_CALIBRATED_STUDENT_H

#include "../sigma/decompose.h"

/** Squared error on epistemic and aleatoric channels (L2 on the σ pair). */
float v43_calibration_loss(const sigma_decomposed_t *student_sigma, const sigma_decomposed_t *teacher_sigma);

/** L_total = L_distill + lambda * L_calibrate */
float v43_student_total_loss(float distill_loss, float calibration_loss, float lambda);

#endif /* CREATION_OS_V43_CALIBRATED_STUDENT_H */
