/*
 * Multimodal σ-gated perception — vision / audio / text backends over HTTP + local llama-server.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_PERCEPTION_H
#define COS_SIGMA_PERCEPTION_H

#include "error_attribution.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    COS_PERCEPTION_TEXT  = 0,
    COS_PERCEPTION_IMAGE = 1,
    COS_PERCEPTION_AUDIO = 2,
};

struct cos_perception_input {
    int      modality;
    char     filepath[512];
    char     text[4096];
    int64_t  timestamp;
};

struct cos_perception_result {
    char                 description[4096];
    float                sigma;
    enum cos_error_source attribution;
    int                  modality;
    int64_t              latency_ms;
};

int cos_perception_init(void);

int cos_perception_process(const struct cos_perception_input *input,
                           struct cos_perception_result       *result);

float cos_perception_confidence(const struct cos_perception_result *result);

#ifdef CREATION_OS_ENABLE_SELF_TESTS
void cos_perception_self_test(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_PERCEPTION_H */
