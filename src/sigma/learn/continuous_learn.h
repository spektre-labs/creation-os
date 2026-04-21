/* ULTRA-8 — unified continuous-learning counters (demo / offline ledger). */

#ifndef COS_ULTRA_CONTINUOUS_LEARN_H
#define COS_ULTRA_CONTINUOUS_LEARN_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void cos_ultra_continuous_emit_status(FILE *fp);

int cos_ultra_continuous_learn_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
