/* AGI-3 — escalation → distill pairs bookkeeping. */

#ifndef COS_SIGMA_AGI_CONTINUOUS_H
#define COS_SIGMA_AGI_CONTINUOUS_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   pairs_at_last_train;
    long  last_train_unix;
} cos_agi_distill_train_state_t;

int cos_agi_distill_count_pairs(const char *path, int *out_n);
int cos_agi_distill_load_train_state(cos_agi_distill_train_state_t *st);
int cos_agi_distill_record_train(int pairs_used);
int cos_agi_distill_emit_status(FILE *fp);
int cos_agi_distill_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
