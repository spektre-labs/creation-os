/*
 * σ-swarm — implementation (HORIZON-4).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "cos_swarm.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float clip01(float x) {
    if (x < 0.f) return 0.f;
    if (x > 1.f) return 1.f;
    return x;
}

void cos_swarm_init(cos_swarm_net_t *n, float lr, float tau_abstain) {
    if (!n) return;
    memset(n, 0, sizeof *n);
    n->lr           = lr > 0.f ? lr : 0.20f;
    n->tau_abstain  = (tau_abstain > 0.f && tau_abstain < 1.f)
                        ? tau_abstain : 0.90f;
}

int cos_swarm_register(cos_swarm_net_t *n, const char *peer_id,
                       const char *endpoint, const char *specialty) {
    if (!n || !peer_id || !endpoint) return -1;
    if (n->n_peers >= COS_SWARM_MAX_PEERS) return -1;
    cos_swarm_peer_t *p = &n->peers[n->n_peers++];
    memset(p, 0, sizeof *p);
    snprintf(p->peer_id, sizeof p->peer_id, "%s", peer_id);
    snprintf(p->endpoint, sizeof p->endpoint, "%s", endpoint);
    snprintf(p->specialty, sizeof p->specialty, "%s",
             specialty ? specialty : "");
    p->trust_ema  = 0.50f;
    p->sigma_mean = 0.50f;
    return 0;
}

int cos_swarm_route(const cos_swarm_net_t *n, const float *sigmas, int n_sig,
                    float *out_min_sigma) {
    if (!n || !sigmas || n_sig < 1 || n_sig != n->n_peers) return -1;
    int best = 0;
    float mn = sigmas[0];
    for (int i = 1; i < n_sig; i++) {
        if (sigmas[i] < mn) {
            mn = sigmas[i];
            best = i;
        }
    }
    if (mn > n->tau_abstain) return -1;
    if (out_min_sigma) *out_min_sigma = mn;
    return best;
}

void cos_swarm_trust_update(cos_swarm_net_t *n, int chosen_idx,
                            float response_sigma) {
    if (!n || chosen_idx < 0 || chosen_idx >= n->n_peers) return;
    cos_swarm_peer_t *p = &n->peers[chosen_idx];
    float t = p->trust_ema + n->lr * (response_sigma - p->trust_ema);
    p->trust_ema = clip01(t);
    /* Exponential moving average of reported σ for dashboard use. */
    p->sigma_mean = p->sigma_mean + n->lr * (response_sigma - p->sigma_mean);
}

unsigned cos_swarm_prompt_hash(const char *prompt) {
    if (!prompt) return 0;
    unsigned h = 5381u;
    for (const unsigned char *p = (const unsigned char *)prompt; *p; p++)
        h = ((h << 5) + h) + *p;
    return h;
}

float cos_swarm_mock_sigma(int peer_index, const char *prompt) {
    /* Bases 0.18, 0.03, 0.15 for indices 0,1,2 — peer 1 always wins
     * after identical tiny perturbation. */
    static const float bases[8] = { 0.18f, 0.03f, 0.15f, 0.20f,
                                    0.11f, 0.09f, 0.14f, 0.16f };
    float b = bases[peer_index & 7];
    unsigned h = cos_swarm_prompt_hash(prompt);
    float jitter = (float)(h % 37u) * 0.0004f;
    return clip01(b + jitter);
}

int cos_swarm_self_test(void) {
    cos_swarm_net_t net;
    cos_swarm_init(&net, 0.20f, 0.90f);
    if (cos_swarm_register(&net, "p0", "localhost:8081", "factual") != 0)
        return 1;
    if (cos_swarm_register(&net, "p1", "localhost:8082", "code") != 0)
        return 2;
    if (cos_swarm_register(&net, "p2", "localhost:8083", "reasoning") != 0)
        return 3;

    const char *prompts[] = {
        "What is 2+2?",
        "Explain recursion",
        "void *p = NULL;",
        "Σ gate",
        "hello swarm",
        "τ accept",
        "BitNet",
        "conformal",
        "K_eff",
        "1 = 1",
    };
    float sigs[COS_SWARM_MAX_PEERS];
    for (int t = 0; t < 10; t++) {
        for (int i = 0; i < net.n_peers; i++)
            sigs[i] = cos_swarm_mock_sigma(i, prompts[t]);
        float mn = 0.f;
        int ch = cos_swarm_route(&net, sigs, net.n_peers, &mn);
        if (ch != 1) return 10 + t;
        if (sigs[1] != mn) return 20 + t;
        cos_swarm_trust_update(&net, ch, sigs[ch]);
    }
    /* Trust EMA should have moved toward low σ for peer 1. */
    if (!(net.peers[1].trust_ema < 0.50f)) return 30;
    return 0;
}
