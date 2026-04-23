/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#define CREATION_OS_ENABLE_SELF_TESTS 1
#define COS_REPUTATION_TEST 1

#include "../../src/sigma/reputation.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    char prev[512];
    prev[0] = '\0';
    const char *oh = getenv("HOME");
    if (oh) snprintf(prev, sizeof prev, "%s", oh);
    assert(setenv("HOME", "/tmp", 1) == 0);

    assert(cos_reputation_self_test() == 0);

    assert(cos_reputation_update("edge", 0.5f, 0, 1) == 0);
    struct cos_reputation g = cos_reputation_get("edge");
    assert(g.total_served >= 1);

    struct cos_reputation rk[8];
    int nr = 0;
    assert(cos_reputation_ranking(rk, 8, &nr) == 0);

    if (prev[0]) (void)setenv("HOME", prev, 1);

    puts("check_reputation_main: OK");
    return 0;
}
