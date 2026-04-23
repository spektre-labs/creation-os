/*
 * Evolution engine harness (four checks + embedded self-test).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "../../src/sigma/evolution_engine.h"

#include <stdlib.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#error CREATION_OS_ENABLE_SELF_TESTS required
#endif

int main(void)
{
    struct cos_evolution_generation r[2];

    (void)setenv("COS_CODEGEN_MAX_PER_DAY", "9999", 1);

    if (cos_evolution_run(0, r) != -1)
        return 50;

    if (setenv("COS_CODEGEN_OFFLINE", "1", 1) != 0)
        return 51;
    if (setenv("COS_EVOLUTION_OFFLINE", "1", 1) != 0)
        return 52;

    if (cos_evolution_run(1, r) != 0)
        return 53;

    cos_evolution_print_report(r, 1);

    if (cos_evolution_append_history(r) != 0)
        return 54;

    return cos_evolution_self_test() != 0 ? 55 : 0;
}
