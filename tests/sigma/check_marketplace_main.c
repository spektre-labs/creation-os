/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#define CREATION_OS_ENABLE_SELF_TESTS 1
#define COS_MARKETPLACE_TEST 1
#define COS_REPUTATION_TEST 1

#include "../../src/sigma/marketplace.h"
#include "../../src/sigma/reputation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
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
    assert(cos_marketplace_self_test() == 0);

    struct cos_service_request rq;
    memset(&rq, 0, sizeof rq);
    snprintf(rq.service_id, sizeof rq.service_id, "%s", "none");
    snprintf(rq.prompt, sizeof rq.prompt, "%s", "ping");
    snprintf(rq.requester_id, sizeof rq.requester_id, "%s", "t");
    rq.max_sigma = 0.5f;

    struct cos_service_response rsp;
    memset(&rsp, 0, sizeof rsp);
    assert(cos_marketplace_request(&rq, &rsp)
           != 0); /* no catalog in fresh /tmp → expect failure */

    struct cos_service s;
    memset(&s, 0, sizeof s);
    snprintf(s.service_id, sizeof s.service_id, "%s", "test.svc");
    snprintf(s.name, sizeof s.name, "%s", "T");
    snprintf(s.provider_id, sizeof s.provider_id, "%s", "p");
    s.sigma_mean = 0.2f;
    snprintf(s.endpoint, sizeof s.endpoint, "%s", "");
    assert(cos_marketplace_register_service(&s) == 0);

    struct cos_service sv[8];
    int n = 0;
    assert(cos_marketplace_browse(sv, 8, &n) == 0);
    assert(n >= 1);

    if (prev[0]) (void)setenv("HOME", prev, 1);

    puts("check_marketplace_main: OK");
    return 0;
}
