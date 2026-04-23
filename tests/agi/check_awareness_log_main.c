/*
 * Awareness log harness (four checks + self-test).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "../../src/sigma/awareness_log.h"
#include "../../src/sigma/consciousness_meter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#error CREATION_OS_ENABLE_SELF_TESTS required
#endif

int main(void)
{
    float tr;
    int   ne;
    char  path[256];
    char  rep[512];

    snprintf(path, sizeof path, "/tmp/cos_aw_suite_%llu.db",
             (unsigned long long)getpid());
    unlink(path);
    if (setenv("COS_AWARENESS_DB", path, 1) != 0)
        return 70;

    if (cos_awareness_trend(&tr, &ne) != 0)
        return 70;

    if (cos_awareness_open() != 0)
        return 72;

    if (cos_awareness_report(rep, sizeof rep) != 0)
        return 73;

    cos_awareness_close();
    unlink(path);

    return cos_awareness_self_test() != 0 ? 74 : 0;
}
