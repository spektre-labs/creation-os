/*
 * σ-A2A — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "a2a.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------ helpers ------------------ */

static void acopy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static float clip01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

/* ------------------ lifecycle ------------------ */

void cos_a2a_init(cos_a2a_network_t *net, const char *self_id) {
    if (!net) return;
    memset(net, 0, sizeof *net);
    acopy(net->self_id, sizeof net->self_id,
          self_id ? self_id : "creation-os-sigma");
    net->tau_block = COS_A2A_TAU_BLOCK;
    net->lr        = COS_A2A_LR;
}

cos_a2a_peer_t *cos_a2a_peer_find(cos_a2a_network_t *net,
                                  const char *agent_id) {
    if (!net || !agent_id) return NULL;
    for (int i = 0; i < net->n_peers; i++) {
        if (strcmp(net->peers[i].agent_id, agent_id) == 0)
            return &net->peers[i];
    }
    return NULL;
}

int cos_a2a_peer_register(cos_a2a_network_t *net,
                          const char *agent_id,
                          const char *agent_card_url,
                          const char *const *capabilities,
                          int n_capabilities,
                          int sigma_gate_declared,
                          int scsl_compliant) {
    if (!net || !agent_id) return COS_A2A_ERR_ARG;
    if (n_capabilities < 0 || n_capabilities > COS_A2A_CAPS_PER_PEER)
        return COS_A2A_ERR_ARG;

    cos_a2a_peer_t *p = cos_a2a_peer_find(net, agent_id);
    if (!p) {
        if (net->n_peers >= COS_A2A_MAX_PEERS) return COS_A2A_ERR_FULL;
        p = &net->peers[net->n_peers++];
        memset(p, 0, sizeof *p);
        p->sigma_trust = COS_A2A_TRUST_INIT;
    }
    acopy(p->agent_id, sizeof p->agent_id, agent_id);
    acopy(p->agent_card_url, sizeof p->agent_card_url,
          agent_card_url ? agent_card_url : "");
    p->n_capabilities = n_capabilities;
    for (int i = 0; i < n_capabilities; i++) {
        acopy(p->capabilities[i], COS_A2A_CAP_MAX,
              capabilities && capabilities[i] ? capabilities[i] : "");
    }
    p->sigma_gate_declared = sigma_gate_declared ? 1 : 0;
    p->scsl_compliant      = scsl_compliant ? 1 : 0;
    return COS_A2A_OK;
}

/* ------------------ exchange ------------------ */

int cos_a2a_request(cos_a2a_network_t *net,
                    const char *agent_id,
                    const char *capability,
                    float sigma_response) {
    if (!net || !agent_id || !capability) return COS_A2A_ERR_ARG;
    cos_a2a_peer_t *p = cos_a2a_peer_find(net, agent_id);
    if (!p) return COS_A2A_ERR_NOPEER;
    if (p->blocklisted) {
        p->exchanges_blocked++;
        return COS_A2A_ERR_BLOCKED;
    }

    /* Capability check — must advertise it. */
    int found = 0;
    for (int i = 0; i < p->n_capabilities; i++) {
        if (strcmp(p->capabilities[i], capability) == 0) { found = 1; break; }
    }
    if (!found) return COS_A2A_ERR_CAP;

    /* EMA update. */
    sigma_response = clip01(sigma_response);
    p->sigma_trust = clip01(
        p->sigma_trust + net->lr * (sigma_response - p->sigma_trust));
    p->exchanges_total++;

    if (p->sigma_trust > net->tau_block) {
        p->blocklisted = 1;
        p->exchanges_blocked++;
        return COS_A2A_ERR_BLOCKED;
    }
    return COS_A2A_OK;
}

/* ------------------ self card ------------------ */

int cos_a2a_self_card(const cos_a2a_network_t *net,
                      const char *const *capabilities,
                      int n_capabilities,
                      char *buf, size_t cap) {
    if (!net || !buf || cap == 0) return COS_A2A_ERR_ARG;
    size_t off = 0;
    int w;
    w = snprintf(buf + off, cap - off,
                 "{\"name\":\"%s\",\"protocol\":\"a2a/1.0\","
                 "\"sigma_gate\":true,\"scsl_compliant\":true,"
                 "\"capabilities\":[",
                 net->self_id);
    if (w < 0 || (size_t)w >= cap - off) return COS_A2A_ERR_ARG;
    off += (size_t)w;
    for (int i = 0; i < n_capabilities; i++) {
        w = snprintf(buf + off, cap - off, "%s\"%s\"",
                     i == 0 ? "" : ",",
                     capabilities[i] ? capabilities[i] : "");
        if (w < 0 || (size_t)w >= cap - off) return COS_A2A_ERR_ARG;
        off += (size_t)w;
    }
    w = snprintf(buf + off, cap - off, "]}");
    if (w < 0 || (size_t)w >= cap - off) return COS_A2A_ERR_ARG;
    off += (size_t)w;
    return (int)off;
}

/* ------------------ self-test ------------------
 *
 * Three peers (good, drifting, hostile).  Exchange 20 calls, assert:
 *   - good peer's σ_trust drops below 0.25
 *   - hostile peer's σ_trust exceeds τ_block and it's blocklisted
 *   - blocklisted peer's subsequent requests return COS_A2A_ERR_BLOCKED
 *   - capability mismatch rejected with COS_A2A_ERR_CAP
 */
int cos_a2a_self_test(void) {
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
        /* good ≈ σ 0.10 */
        cos_a2a_request(&net, "peer-good",    "factual_qa", 0.10f);
        /* drifting ≈ σ 0.55 */
        cos_a2a_request(&net, "peer-drift",   "factual_qa", 0.55f);
        /* hostile ≈ σ 0.95 */
        cos_a2a_request(&net, "peer-hostile", "factual_qa", 0.95f);
    }

    cos_a2a_peer_t *g = cos_a2a_peer_find(&net, "peer-good");
    cos_a2a_peer_t *d = cos_a2a_peer_find(&net, "peer-drift");
    cos_a2a_peer_t *h = cos_a2a_peer_find(&net, "peer-hostile");
    if (!g || !d || !h) return -1;
    if (!(g->sigma_trust < 0.25f)) return -2;
    if (!(h->blocklisted))          return -3;
    if (!(h->sigma_trust > net.tau_block)) return -4;

    /* Blocklisted peer further requests → ERR_BLOCKED, exchanges_blocked bumps */
    int before = h->exchanges_blocked;
    int rc = cos_a2a_request(&net, "peer-hostile", "factual_qa", 0.10f);
    if (rc != COS_A2A_ERR_BLOCKED)     return -5;
    if (h->exchanges_blocked <= before) return -6;

    /* Capability mismatch */
    int rc2 = cos_a2a_request(&net, "peer-good", "video_generation", 0.10f);
    if (rc2 != COS_A2A_ERR_CAP) return -7;

    /* Self-card encodes declared capabilities */
    const char *my_caps[] = { "factual_qa", "code_review", "sigma_measurement" };
    char card[512];
    int n = cos_a2a_self_card(&net, my_caps, 3, card, sizeof card);
    if (n <= 0) return -8;
    if (!strstr(card, "creation-os-sigma")) return -9;
    if (!strstr(card, "\"a2a/1.0\""))       return -10;
    if (!strstr(card, "sigma_measurement")) return -11;

    /* Unknown peer */
    int rc3 = cos_a2a_request(&net, "phantom", "factual_qa", 0.10f);
    if (rc3 != COS_A2A_ERR_NOPEER) return -12;
    return 0;
}
