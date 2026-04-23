/*
 * Codebase evolution driver — wraps σ-codegen proposals across generations.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_EVOLUTION_ENGINE_H
#define COS_SIGMA_EVOLUTION_ENGINE_H

#include "codegen.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cos_evolution_generation {
    int                         gen_number;
    struct cos_codegen_proposal proposals[8];
    int                         n_proposals;
    float                       fitness_before;
    float                       fitness_after;
    int                         accepted_count;
    int                         rejected_count;
};

int cos_evolution_run(int                    n_generations,
                      struct cos_evolution_generation *results);

void cos_evolution_print_report(const struct cos_evolution_generation *results,
                                int                                  n_gen);

int cos_evolution_append_history(const struct cos_evolution_generation *g);

int cos_evolution_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_EVOLUTION_ENGINE_H */
