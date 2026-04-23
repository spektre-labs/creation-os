/*
 * Thin harness for ULTRA-8 TTT runtime self-test.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "ttt_runtime.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int f = cos_ttt_runtime_self_test();
    if (f != 0)
        fprintf(stderr, "check_ttt_runtime: %d failure(s)\n", f);
    return f != 0 ? 1 : 0;
}
