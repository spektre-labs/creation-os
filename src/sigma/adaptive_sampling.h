/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Optional early stop for semantic σ: after the first auxiliary sample,
 * skip the second HTTP call when pair-σ is already extreme (low or high).
 * Inspired by adaptive multi-sample entropy estimators; default OFF.
 */
#ifndef COS_ADAPTIVE_SAMPLING_H
#define COS_ADAPTIVE_SAMPLING_H

#ifdef __cplusplus
extern "C" {
#endif

/** 1 if COS_SEMANTIC_SIGMA_ADAPTIVE=1 (exact). */
int cos_semantic_adaptive_enabled(void);

/** Thresholds on pair-σ (1 − Jaccard on first sentences); inclusive edges stop early. */
float cos_adaptive_sigma_env_low(void);
float cos_adaptive_sigma_env_high(void);

/** After primary + one paraphrase: 1 → take third sample; 0 → stop at two. */
int cos_adaptive_take_third_sample(float pair_sigma);

#ifdef __cplusplus
}
#endif
#endif /* COS_ADAPTIVE_SAMPLING_H */
