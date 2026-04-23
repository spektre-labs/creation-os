/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#ifndef COS_STIGMERGY_H
#define COS_STIGMERGY_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_stigmergy_trail {
    uint64_t prompt_hash;
    char     response[4096];
    float    sigma;
    char     peer_id[64];
    int64_t  timestamp;
    float    decay;
};

int cos_stigmergy_init(void);
void cos_stigmergy_shutdown(void);

int cos_stigmergy_leave_trail(const struct cos_stigmergy_trail *trail);

/** Returns NULL if none; owned by library until next call. */
const struct cos_stigmergy_trail *cos_stigmergy_check(uint64_t prompt_hash);

int cos_stigmergy_decay(float decay_rate);
int cos_stigmergy_reinforce(uint64_t prompt_hash, float boost);

void cos_stigmergy_fprint_active(FILE *f, int max_rows);

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(COS_STIGMERGY_TEST)
int cos_stigmergy_self_test(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* COS_STIGMERGY_H */
