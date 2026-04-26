/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Tiered Ollama cascade: try small models first; intra-tier semantic σ
 * from cos_semantic_sigma_ex decides whether to escalate.
 */
#ifndef COS_MODEL_CASCADE_H
#define COS_MODEL_CASCADE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Writes NUL-terminated response into response_out; on abstain tier_used=-1.
 * Returns 0 on success (including abstain with message), -1 on args. */
int cos_cascade_query(int port, const char *prompt, const char *system_prompt,
                      char *response_out, size_t response_cap, float *sigma_out,
                      int *tier_used);

#ifdef __cplusplus
}
#endif

#endif /* COS_MODEL_CASCADE_H */
