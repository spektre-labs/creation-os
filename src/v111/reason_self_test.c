/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v111.2 σ-Reason self-test harness.  Runs the pure-C smoke checks that
 * do not require a loaded model, and exits 0 iff all PASS.
 */
#include "reason.h"
#include <stdio.h>

int main(void)
{
    return cos_v111_reason_self_test();
}
