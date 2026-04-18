/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v150 σ-Swarm-Intelligence CLI wrapper.
 */
#include "swarm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *p) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --debate TOPIC --question TEXT [--seed N] [--verify]\n"
        "       %s --route  TOPIC [--seed N]\n", p, p, p);
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 2; }

    if (!strcmp(argv[1], "--self-test")) {
        int rc = cos_v150_self_test();
        printf("{\"kernel\":\"v150\",\"self_test\":%s,\"rc\":%d}\n",
               rc == 0 ? "\"pass\"" : "\"fail\"", rc);
        return rc;
    }

    int do_debate = 0, do_route = 0, do_verify = 0;
    const char *topic = NULL;
    const char *question = "default swarm query";
    uint64_t seed = 0x150UL;
    float tau = 0.30f;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "--debate") && i + 1 < argc) { do_debate = 1; topic = argv[++i]; }
        else if (!strcmp(a, "--route") && i + 1 < argc) { do_route = 1; topic = argv[++i]; }
        else if (!strcmp(a, "--question") && i + 1 < argc) question = argv[++i];
        else if (!strcmp(a, "--seed")     && i + 1 < argc) seed = strtoull(argv[++i], NULL, 0);
        else if (!strcmp(a, "--tau")      && i + 1 < argc) tau  = (float)atof(argv[++i]);
        else if (!strcmp(a, "--verify")) do_verify = 1;
        else if (a[0] == '-' && strcmp(a, argv[0]) != 0) { usage(argv[0]); return 2; }
    }

    cos_v150_swarm_t s;
    cos_v150_swarm_init(&s, seed);
    cos_v150_seed_defaults(&s);

    if (do_route) {
        if (!topic) { usage(argv[0]); return 2; }
        int id = cos_v150_route_by_topic(&s, topic);
        printf("{\"kernel\":\"v150\",\"mode\":\"route\","
               "\"topic\":\"%s\",\"specialist_id\":%d,"
               "\"name\":\"%s\",\"specialist_topic\":\"%s\"}\n",
               topic, id, s.spec[id].name, s.spec[id].topic);
        return 0;
    }

    if (do_debate) {
        if (!topic) { usage(argv[0]); return 2; }
        cos_v150_debate_t d;
        if (cos_v150_debate_run(&s, question, topic, &d) != 0) return 3;
        if (do_verify) cos_v150_adversarial_verify(&d, tau);
        char buf[8192];
        cos_v150_debate_to_json(&d, buf, sizeof(buf));
        printf("%s\n", buf);
        return 0;
    }

    usage(argv[0]);
    return 2;
}
