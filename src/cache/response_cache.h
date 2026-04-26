/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Disk-backed prompt cache (~/.cos/cache/) keyed by SHA-256(prompt+model).
 */
#ifndef COS_CACHE_RESPONSE_CACHE_H
#define COS_CACHE_RESPONSE_CACHE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char     prompt_hash[65];
    char     response[4096];
    float    sigma;
    char     action[16];
    uint64_t timestamp;
} cos_cache_entry_t;

/** 0 on hit (entry filled), -1 on miss. */
int cos_cache_lookup(const char *prompt, const char *model, cos_cache_entry_t *out);

void cos_cache_store(const char *prompt, const char *model, const char *response,
                     float sigma, const char *action);

#ifdef __cplusplus
}
#endif
#endif /* COS_CACHE_RESPONSE_CACHE_H */
