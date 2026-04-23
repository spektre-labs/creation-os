/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#define CREATION_OS_ENABLE_SELF_TESTS 1
#define COS_SWARM_COORD_TEST 1

#include "../../src/sigma/swarm_coordinator.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    assert(cos_swarm_coord_self_test() == 0);

    struct cos_swarm_vote v[2];
    memset(v, 0, sizeof v);
    snprintf(v[0].peer_id, sizeof v[0].peer_id, "%s", "z");
    snprintf(v[0].response, sizeof v[0].response, "%s", "alpha beta");
    v[0].sigma = 0.99f;
    snprintf(v[1].peer_id, sizeof v[1].peer_id, "%s", "y");
    snprintf(v[1].response, sizeof v[1].response, "%s", "alpha beta gamma");
    v[1].sigma = 0.11f;

    struct cos_swarm_result r;
    memset(&r, 0, sizeof r);
    assert(cos_swarm_vote_lowest_sigma(v, 2, &r) == 0);
    assert(r.sigma_best < 0.2f);

    assert(cos_swarm_coord_init() == 0);
    struct cos_swarm_peer p;
    memset(&p, 0, sizeof p);
    snprintf(p.peer_id, sizeof p.peer_id, "%s", "rm");
    snprintf(p.endpoint, sizeof p.endpoint, "%s", "127.0.0.1:9");
    p.trust = 0.5f;
    assert(cos_swarm_coord_add_peer(&p) == 0);
    int n = 0;
    assert(cos_swarm_coord_peers_list(NULL, 0, &n) != 0);
    cos_swarm_coord_shutdown();

    puts("check_swarm_coordinator_main: OK");
    return 0;
}
