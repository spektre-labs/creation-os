/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Decentralized σ-based reputation (SQLite at ~/.cos/marketplace/).
 */

#ifndef COS_REPUTATION_H
#define COS_REPUTATION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_reputation {
    char  node_id[64];
    float sigma_mean_served;
    float reliability;
    int   total_served;
    int   total_verified;
    int   total_rejected;
    float trust_composite;
};

int cos_reputation_init(void);
void cos_reputation_shutdown(void);

/** Record one interaction: verified=receipt OK, succeeded=request completed well. */
int cos_reputation_update(const char *node_id, float sigma, int verified,
                          int succeeded);

struct cos_reputation cos_reputation_get(const char *node_id);

int cos_reputation_ranking(struct cos_reputation *nodes, int max, int *n);

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(COS_REPUTATION_TEST)
int cos_reputation_self_test(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* COS_REPUTATION_H */
