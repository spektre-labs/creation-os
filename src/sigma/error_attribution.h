/*
 * σ-error attribution — maps multi-channel uncertainty to an
 * interpretable source + remediation hint (diagnostic-only).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_ERROR_ATTRIBUTION_H
#define COS_SIGMA_ERROR_ATTRIBUTION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum cos_error_source {
    COS_ERR_NONE = 0,
    COS_ERR_EPISTEMIC,
    COS_ERR_ALEATORIC,
    COS_ERR_REASONING,
    COS_ERR_MEMORY,
    COS_ERR_NOVEL_DOMAIN,
    COS_ERR_INJECTION
};

struct cos_error_attribution {
    enum cos_error_source source;
    float confidence;
    char reason[256];
    char fix[256];
};

struct cos_error_attribution cos_error_attribute(
    float sigma_logprob,
    float sigma_entropy,
    float sigma_consistency,
    float meta_perception);

char *cos_error_attribution_to_json(const struct cos_error_attribution *a);

int cos_error_attribution_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_ERROR_ATTRIBUTION_H */
