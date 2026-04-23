/*
 * σ-gated code generation — sandbox compile/test before any integration.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_CODEGEN_H
#define COS_SIGMA_CODEGEN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_CODEGEN_MAX_CODE     16384
#define COS_CODEGEN_MAX_TEST     8192
#define COS_CODEGEN_MAX_PENDING  64

struct cos_codegen_proposal {
    char  filename[256];
    char  code[COS_CODEGEN_MAX_CODE];
    char  test_code[COS_CODEGEN_MAX_TEST];
    float sigma_confidence;
    int   compilation_ok;
    int   tests_passed;
    int   tests_total;
    float sigma_after_test;
    int   approved;
    char  rejection_reason[256];
    int   proposal_id;
};

float cos_codegen_tau(void);
void  cos_codegen_set_tau(float tau);

int cos_codegen_propose(const char *goal, struct cos_codegen_proposal *proposal);

int cos_codegen_compile_sandbox(struct cos_codegen_proposal *p);

int cos_codegen_test_sandbox(struct cos_codegen_proposal *p);

int cos_codegen_frama_check(struct cos_codegen_proposal *p);

int cos_codegen_check_regression(struct cos_codegen_proposal *p);

int cos_codegen_gate(struct cos_codegen_proposal *p);

int cos_codegen_register(struct cos_codegen_proposal *p);

int cos_codegen_list(struct cos_codegen_proposal *out, int max, int *n_out);

int cos_codegen_get(int id, struct cos_codegen_proposal *out);

int cos_codegen_approve(struct cos_codegen_proposal *p);

int cos_codegen_integrate(const struct cos_codegen_proposal *p,
                          const char *target_dir);

int cos_codegen_reject(struct cos_codegen_proposal *p, const char *reason);

int cos_codegen_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_CODEGEN_H */
