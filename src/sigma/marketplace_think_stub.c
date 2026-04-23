/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Weak cos_think_run for lightweight check binaries.  Full `cos` links the
 * strong definition from cos_think.c instead.
 */

#include "../cli/cos_think.h"

__attribute__((weak)) int cos_think_run(const char *goal, int max_rounds,
                                        struct cos_think_result *result)
{
    (void)goal;
    (void)max_rounds;
    (void)result;
    return -1;
}
