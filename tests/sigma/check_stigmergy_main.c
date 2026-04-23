/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#define CREATION_OS_ENABLE_SELF_TESTS 1
#define COS_STIGMERGY_TEST 1

#include "../../src/sigma/stigmergy.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    char prev[512];
    prev[0] = '\0';
    const char *oh = getenv("HOME");
    if (oh)
        snprintf(prev, sizeof prev, "%s", oh);
    assert(setenv("HOME", "/tmp", 1) == 0);

    assert(cos_stigmergy_self_test() == 0);

    assert(cos_stigmergy_init() == 0);
    assert(cos_stigmergy_decay(0.5f) == 0);
    cos_stigmergy_shutdown();

    if (prev[0])
        (void)setenv("HOME", prev, 1);

    puts("check_stigmergy_main: OK");
    return 0;
}
