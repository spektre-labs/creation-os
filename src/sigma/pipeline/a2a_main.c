/*
 * σ-A2A demo — three peers, 20 rounds of exchanges, deterministic
 * JSON envelope for CI.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "a2a.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    int st = cos_a2a_self_test();

    cos_a2a_network_t net;
    cos_a2a_init(&net, "creation-os-sigma");
    const char *good_caps[]    = { "factual_qa", "sigma_measurement" };
    const char *drift_caps[]   = { "factual_qa" };
    const char *hostile_caps[] = { "factual_qa", "code_review" };
    cos_a2a_peer_register(&net, "peer-good",    "https://good/agent.json",
                          good_caps,    2, 1, 1);
    cos_a2a_peer_register(&net, "peer-drift",   "https://drift/agent.json",
                          drift_caps,   1, 0, 0);
    cos_a2a_peer_register(&net, "peer-hostile", "https://hostile/agent.json",
                          hostile_caps, 2, 0, 0);

    for (int i = 0; i < 20; i++) {
        cos_a2a_request(&net, "peer-good",    "factual_qa", 0.10f);
        cos_a2a_request(&net, "peer-drift",   "factual_qa", 0.55f);
        cos_a2a_request(&net, "peer-hostile", "factual_qa", 0.95f);
    }

    const char *my_caps[] = { "factual_qa", "code_review", "sigma_measurement" };
    char card[512];
    cos_a2a_self_card(&net, my_caps, 3, card, sizeof card);

    printf("{\n");
    printf("  \"kernel\": \"sigma_a2a\",\n");
    printf("  \"self_test\": %d,\n", st);
    printf("  \"self_id\": \"%s\",\n", net.self_id);
    printf("  \"tau_block\": %.4f,\n", (double)net.tau_block);
    printf("  \"lr\": %.4f,\n", (double)net.lr);
    printf("  \"self_card\": %s,\n", card);
    printf("  \"peers\": [\n");
    for (int i = 0; i < net.n_peers; i++) {
        const cos_a2a_peer_t *p = &net.peers[i];
        printf("    { \"agent_id\": \"%s\","
               " \"agent_card_url\": \"%s\","
               " \"sigma_gate_declared\": %s,"
               " \"scsl_compliant\": %s,"
               " \"capabilities\": [",
               p->agent_id, p->agent_card_url,
               p->sigma_gate_declared ? "true" : "false",
               p->scsl_compliant      ? "true" : "false");
        for (int j = 0; j < p->n_capabilities; j++) {
            printf("%s\"%s\"", j == 0 ? "" : ",", p->capabilities[j]);
        }
        printf("], \"sigma_trust\": %.4f, \"exchanges_total\": %d,"
               " \"exchanges_blocked\": %d, \"blocklisted\": %s }%s\n",
               (double)p->sigma_trust, p->exchanges_total,
               p->exchanges_blocked,
               p->blocklisted ? "true" : "false",
               i + 1 == net.n_peers ? "" : ",");
    }
    printf("  ]\n");
    printf("}\n");
    return st == 0 ? 0 : 1;
}
