/*
 * σ-codegen harness (eight checks + embedded self-test).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "../../src/sigma/codegen.h"

#include <stdlib.h>
#include <string.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#error CREATION_OS_ENABLE_SELF_TESTS required
#endif

int main(void)
{
    struct cos_codegen_proposal P, Q, L[COS_CODEGEN_MAX_PENDING];
    int                         n;

    (void)setenv("COS_CODEGEN_MAX_PER_DAY", "9999", 1);

    if (cos_codegen_propose(NULL, &P) != -1)
        return 30;

    (void)cos_codegen_tau();
    cos_codegen_set_tau(0.41f);
    if (cos_codegen_tau() < 0.4f || cos_codegen_tau() > 0.5f)
        return 31;

    if (setenv("COS_CODEGEN_OFFLINE", "1", 1) != 0)
        return 32;

    memset(&P, 0, sizeof P);
    if (cos_codegen_propose("unit goal", &P) != 0)
        return 33;

    if (cos_codegen_compile_sandbox(&P) != 0)
        return 34;

    if (cos_codegen_test_sandbox(&P) != 0)
        return 35;

    if (P.tests_passed != 1)
        return 36;

    if (cos_codegen_gate(&P) != 0)
        return 37;

    if (cos_codegen_register(&P) != 0)
        return 38;

    if (cos_codegen_get(P.proposal_id, &Q) != 0)
        return 39;

    if (cos_codegen_list(L, COS_CODEGEN_MAX_PENDING, &n) != 0 || n < 1)
        return 40;

    if (cos_codegen_reject(&Q, "check harness") != 0)
        return 41;

    return cos_codegen_self_test() != 0 ? 42 : 0;
}
