/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Automated multi-teacher "proconductor" — same prompt to every available
 * cloud teacher, local σ on each answer, optional consensus + synthesis.
 */

#ifndef COS_PROCONDUCTOR_H
#define COS_PROCONDUCTOR_H

#include "distill.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cos_proconductor_session {
    char  goal[1024];
    struct cos_distill_example examples[8];
    int   n_examples;
    float sigma_consensus;
    float sigma_best;
    int   consensus;
    char  synthesis[4096];
};

int cos_proconductor_run(const char *goal, const char *only_teachers,
                         struct cos_proconductor_session *session);

void cos_proconductor_print_report(
    const struct cos_proconductor_session *session);

int cos_proconductor_load_history(int max_rows);

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(CREATION_OS_PROCONDUCTOR_TEST)
int cos_proconductor_self_test(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* COS_PROCONDUCTOR_H */
