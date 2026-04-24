/*
 * Autonomous σ-gated web learning (idle Ω-loop + cos learn CLI).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_LEARN_ENGINE_H
#define COS_SIGMA_LEARN_ENGINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_learn_task {
    uint64_t domain_hash;
    char     topic[256];
    float    current_sigma;
    float    target_sigma;
    int      priority;
};

struct cos_learn_result {
    uint64_t domain_hash;
    char     source_url[512];
    char     summary[2048];
    float    sigma_before;
    float    sigma_after;
    int      verified;
    int64_t  timestamp;
};

int cos_learn_init(void);

void cos_learn_shutdown(void);

int cos_learn_identify_gaps(struct cos_learn_task *tasks, int max, int *n);

int cos_learn_research(const struct cos_learn_task *task,
                       struct cos_learn_result *result);

int cos_learn_verify(const struct cos_learn_result *result);

int cos_learn_store(const struct cos_learn_result *result);

void cos_learn_report(void);

int cos_learn_main(int argc, char **argv);

int cos_learn_engine_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_LEARN_ENGINE_H */
