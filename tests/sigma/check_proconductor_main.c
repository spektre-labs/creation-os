/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#define CREATION_OS_ENABLE_SELF_TESTS 1
#define CREATION_OS_PROCONDUCTOR_TEST 1

#include "../../src/sigma/proconductor.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    struct cos_proconductor_session Z;
    memset(&Z, 0, sizeof Z);
    cos_proconductor_print_report(&Z);

    char prev[512];
    prev[0] = '\0';
    const char *oh = getenv("HOME");
    if (oh) snprintf(prev, sizeof prev, "%s", oh);
    assert(setenv("HOME", "/tmp", 1) == 0);
    assert(setenv("COS_BITNET_SERVER_EXTERNAL", "1", 1) == 0);
    assert(setenv("COS_BITNET_SERVER_PORT", "65529", 1) == 0);
    cos_distill_shutdown();

    assert(cos_proconductor_self_test() == 0);

    struct cos_proconductor_session S;
    memset(&S, 0, sizeof S);
    assert(cos_proconductor_run("pc test sigma", NULL, &S) == 0);

    cos_proconductor_print_report(&S);

    assert(cos_proconductor_load_history(2) == 0);

    cos_distill_shutdown();
    if (prev[0]) (void)setenv("HOME", prev, 1);
    return 0;
}
