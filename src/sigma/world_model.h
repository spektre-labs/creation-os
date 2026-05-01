/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_world_belief {
    char subject[128];
    char predicate[128];
    char object[128];
    float sigma;
    int64_t last_verified;
};

int cos_world_query_belief(const char *subject,
    struct cos_world_belief *beliefs, int max, int *n_found);

float cos_world_confidence(const char *subject);

int cos_world_update_belief(const char *subject, const char *predicate,
    const char *object, float sigma);

#ifdef CREATION_OS_ENABLE_SELF_TESTS
void cos_world_self_test(void);
#endif

#ifdef __cplusplus
}
#endif
