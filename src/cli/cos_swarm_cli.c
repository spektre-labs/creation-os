/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * cos swarm — σ-coordinated HTTP fan-out + stigmergy trails (CLI front).
 */

#include "cos_swarm_cli.h"

#include "../sigma/stigmergy.h"
#include "../sigma/swarm_coordinator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void usage(FILE *o)
{
    fputs(
        "usage:\n"
        "  cos swarm status\n"
        "  cos swarm peers\n"
        "  cos swarm trails\n"
        "  cos swarm add HOST:PORT\n"
        "  cos swarm --goal \"...\" [--strategy lowest|majority|weighted]\n"
        "  cos swarm --self-test\n",
        o);
}

static void print_peer_table(void)
{
    struct cos_swarm_peer pp[COS_SWARM_MAX_PEERS_COORD];
    int                  n = 0;
    if (cos_swarm_coord_peers_list(pp, (int)(sizeof pp / sizeof pp[0]), &n)
        != 0) {
        fputs("cos swarm: peer list failed\n", stderr);
        return;
    }
    printf("%d peer(s)\n", n);
    for (int i = 0; i < n; i++) {
        printf(
            "  %-24s  %-42s  σ_mean=%.4f  trust=%.4f  avail=%d  rsp=%d  "
            "best=%d  last_seen_ms=%lld\n",
            pp[i].peer_id, pp[i].endpoint, (double)pp[i].sigma_mean,
            (double)pp[i].trust, pp[i].available, pp[i].responses,
            pp[i].best_count, (long long)pp[i].last_seen_ms);
    }
}

static int cmd_status(void)
{
    if (cos_swarm_coord_init() != 0) {
        fputs("cos swarm: init failed\n", stderr);
        return 2;
    }
    fputs("σ-swarm coordinator\n", stdout);
    print_peer_table();
    cos_swarm_coord_shutdown();
    return 0;
}

static int cmd_peers(void)
{
    if (cos_swarm_coord_init() != 0) {
        fputs("cos swarm: init failed\n", stderr);
        return 2;
    }
    print_peer_table();
    cos_swarm_coord_shutdown();
    return 0;
}

static int cmd_trails(void)
{
    if (cos_stigmergy_init() != 0) {
        fputs("cos swarm: stigmergy db failed\n", stderr);
        return 2;
    }
    fputs("active stigmergy trails (most recent first)\n", stdout);
    cos_stigmergy_fprint_active(stdout, 64);
    cos_stigmergy_shutdown();
    return 0;
}

static int cmd_add(const char *hostport)
{
    if (!hostport || !hostport[0]) {
        usage(stderr);
        return 2;
    }
    struct cos_swarm_peer p;
    memset(&p, 0, sizeof p);
    snprintf(p.peer_id, sizeof p.peer_id, "%s", hostport);
    snprintf(p.endpoint, sizeof p.endpoint, "%s", hostport);
    p.sigma_mean = 0.5f;
    p.trust      = 0.5f;
    p.available  = 1;
    if (cos_swarm_coord_init() != 0) {
        fputs("cos swarm: init failed\n", stderr);
        return 2;
    }
    if (cos_swarm_coord_add_peer(&p) != 0) {
        fputs("cos swarm: add_peer failed\n", stderr);
        cos_swarm_coord_shutdown();
        return 2;
    }
    printf("added peer %s\n", hostport);
    cos_swarm_coord_shutdown();
    return 0;
}

static int parse_strategy(const char *s, enum cos_swarm_vote_strategy *out)
{
    if (!s || !strcasecmp(s, "lowest") || !strcasecmp(s, "lowest_sigma")) {
        *out = COS_SWARM_VOTE_LOWEST_SIGMA;
        return 0;
    }
    if (!strcasecmp(s, "majority")) {
        *out = COS_SWARM_VOTE_MAJORITY;
        return 0;
    }
    if (!strcasecmp(s, "weighted") || !strcasecmp(s, "sigma_weighted")) {
        *out = COS_SWARM_VOTE_SIGMA_WEIGHTED;
        return 0;
    }
    return -1;
}

static int run_goal(const char *goal, enum cos_swarm_vote_strategy st)
{
    if (!goal || !goal[0]) return 2;
    if (cos_swarm_coord_init() != 0) {
        fputs("cos swarm: init failed\n", stderr);
        return 2;
    }
    struct cos_swarm_result r;
    memset(&r, 0, sizeof r);
    int bc = cos_swarm_broadcast(goal, st, &r);
    if (bc != 0) {
        fprintf(stderr, "cos swarm: broadcast failed (%d); need registered "
                        "peers (see `cos swarm add`)\n",
                bc);
        cos_swarm_coord_shutdown();
        return 2;
    }
    printf("method=%s  σ_best=%.4f  consensus=%s  σ_consensus=%.4f  "
           "votes=%d\n",
           r.method, (double)r.sigma_best, r.consensus ? "yes" : "no",
           (double)r.sigma_consensus, r.n_votes);
    printf("--- best ---\n%s\n", r.best_response);
    for (int i = 0; i < r.n_votes && i < COS_SWARM_MAX_VOTES; i++) {
        printf("vote %d: peer=%s σ=%.4f latency_ms=%lld\n", i + 1,
               r.votes[i].peer_id, (double)r.votes[i].sigma,
               (long long)r.votes[i].latency_ms);
    }
    cos_swarm_coord_shutdown();
    return 0;
}

int cos_swarm_main(int argc, char **argv)
{
    const char                *goal     = NULL;
    const char                *strat_s  = NULL;
    int                        selftest = 0;
    enum cos_swarm_vote_strategy st;

    if (argc >= 1 && !strcmp(argv[0], "status")) return cmd_status();
    if (argc >= 1 && !strcmp(argv[0], "peers")) return cmd_peers();
    if (argc >= 1 && !strcmp(argv[0], "trails")) return cmd_trails();
    if (argc >= 1 && !strcmp(argv[0], "add")) {
        if (argc < 2) {
            usage(stderr);
            return 2;
        }
        return cmd_add(argv[1]);
    }

    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--goal") && i + 1 < argc)
            goal = argv[++i];
        else if (!strcmp(argv[i], "--strategy") && i + 1 < argc)
            strat_s = argv[++i];
        else if (!strcmp(argv[i], "--self-test"))
            selftest = 1;
        else if (!strcmp(argv[i], "--peers") && i + 1 < argc)
            ++i; /* advisory: fan-out uses registered peers */
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(stdout);
            return 0;
        }
    }

    if (selftest) {
        if (cos_swarm_coord_self_test() != 0 || cos_stigmergy_self_test() != 0) {
            fputs("cos swarm: --self-test failed\n", stderr);
            return 2;
        }
        fputs("cos swarm: --self-test OK\n", stdout);
        return 0;
    }

    if (goal) {
        if (parse_strategy(strat_s ? strat_s : "lowest", &st) != 0) {
            fputs("cos swarm: bad --strategy\n", stderr);
            return 2;
        }
        return run_goal(goal, st);
    }

    if (argc == 0) {
        usage(stdout);
        return 0;
    }

    fprintf(stderr, "cos swarm: unknown arguments; try `cos swarm --help`\n");
    usage(stderr);
    return 2;
}
