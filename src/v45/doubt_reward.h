/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v45 lab: “Rewarding doubt” — RLVR-style scalar from correctness + epistemic σ + self-report alignment.
 */
#ifndef CREATION_OS_V45_DOUBT_REWARD_H
#define CREATION_OS_V45_DOUBT_REWARD_H

/** is_correct: 1 true, 0 false; other values treated as unknown (falls through to calibration bonus path). */
float v45_doubt_reward(const char *response, int is_correct, float sigma_epistemic, float self_reported_confidence);

#endif /* CREATION_OS_V45_DOUBT_REWARD_H */
